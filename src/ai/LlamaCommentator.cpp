#include "ai/LlamaCommentator.hpp"
#include "core/Logger.hpp"

#include <algorithm>
#include <format>
#include <llama.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <thread>
#include <vector>

namespace rps::ai {

namespace {

std::string build_prompt(const CommentaryContext& ctx) {
    const nlohmann::json round_json = {
        {"round", ctx.round.round_number},
        {"player_move", game::to_string(ctx.round.player_move)},
        {"ai_move", game::to_string(ctx.round.ai_move)},
        {"result", game::to_string(ctx.round.result)},
        {"difficulty", game::to_string(ctx.round.difficulty)},
        {"player_wins", ctx.stats.player_wins},
        {"ai_wins", ctx.stats.ai_wins},
        {"streak", ctx.stats.current_streak}
    };

    const char* lang_instr = (ctx.language == Language::Ru)
        ? "Reply in Russian (1-2 short witty esports sentences)."
        : "Reply in English (1-2 short witty esports sentences).";

    return std::format(
        "<|im_start|>system\n"
        "You are CYBER-CASTER, a sharp, witty AI esports commentator for Rock-Paper-Scissors. "
        "{} Never use hashtags or emojis.\n<|im_end|>\n"
        "<|im_start|>user\nRound state: {}\nComment:<|im_end|>\n"
        "<|im_start|>assistant\n",
        lang_instr,
        round_json.dump()
    );
}

} // namespace

struct LlamaCommentator::Impl {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;

    std::jthread worker;
    std::mutex mutex;
    std::string status = "No GGUF model loaded.";
    std::optional<CommentatorMessage> pending_msg;
    bool has_update = false;

    ~Impl() {
        if (worker.joinable()) {
            worker.request_stop();
            worker.join();
        }
        if (ctx != nullptr) {
            llama_free(ctx);
        }
        if (model != nullptr) {
            llama_model_free(model);
        }
    }
};

LlamaCommentator::LlamaCommentator() : impl_(std::make_unique<Impl>()) {}
LlamaCommentator::~LlamaCommentator() = default;

std::expected<void, std::string> LlamaCommentator::init(const std::filesystem::path& model_path) {
    if (!std::filesystem::exists(model_path)) {
        std::scoped_lock lock(impl_->mutex);
        impl_->status = std::format("GGUF model not found at {}", model_path.string());
        available_.store(false, std::memory_order_release);
        return std::unexpected(impl_->status);
    }

    llama_backend_init();

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = 99;

    llama_model* model = llama_model_load_from_file(model_path.string().c_str(), mparams);
    if (model == nullptr) {
        std::scoped_lock lock(impl_->mutex);
        impl_->status = std::format("Failed to load GGUF model: {}", model_path.string());
        available_.store(false, std::memory_order_release);
        return std::unexpected(impl_->status);
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx = 1024;
    cparams.n_batch = 256;
    cparams.n_threads = 4;
    cparams.n_threads_batch = 4;

    llama_context* ctx = llama_init_from_model(model, cparams);
    if (ctx == nullptr) {
        llama_model_free(model);
        std::scoped_lock lock(impl_->mutex);
        impl_->status = "Failed to initialize llama_context.";
        available_.store(false, std::memory_order_release);
        return std::unexpected(impl_->status);
    }

    {
        std::scoped_lock lock(impl_->mutex);
        if (impl_->ctx) llama_free(impl_->ctx);
        if (impl_->model) llama_model_free(impl_->model);
        impl_->model = model;
        impl_->ctx = ctx;
        impl_->vocab = llama_model_get_vocab(model);
        impl_->status = std::format("Local LLM ready: {}", model_path.filename().string());
    }
    available_.store(true, std::memory_order_release);
    core::Logger::info("Loaded local GGUF LLM: {}", model_path.string());
    return {};
}

void LlamaCommentator::request_commentary(const CommentaryContext& ctx) {
    if (!available_.load(std::memory_order_acquire)) {
        fallback_.request_commentary(ctx);
        if (auto msg = fallback_.poll_update()) {
            std::scoped_lock lock(impl_->mutex);
            impl_->pending_msg = std::move(msg);
            impl_->has_update = true;
        }
        return;
    }

    if (impl_->worker.joinable()) {
        impl_->worker.request_stop();
        impl_->worker.join();
    }

    busy_.store(true, std::memory_order_release);
    impl_->worker = std::jthread([this, ctx](std::stop_token st) {
        const std::string prompt = build_prompt(ctx);

        std::vector<llama_token> tokens(prompt.size() + 16);
        const int n_tokens = llama_tokenize(
            impl_->vocab,
            prompt.c_str(),
            static_cast<int32_t>(prompt.size()),
            tokens.data(),
            static_cast<int32_t>(tokens.size()),
            true,
            true
        );

        if (n_tokens <= 0) {
            busy_.store(false, std::memory_order_release);
            return;
        }
        tokens.resize(static_cast<std::size_t>(n_tokens));

        llama_memory_clear(llama_get_memory(impl_->ctx), true);
        llama_batch batch = llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()));
        if (llama_decode(impl_->ctx, batch) != 0) {
            busy_.store(false, std::memory_order_release);
            return;
        }

        auto sparams = llama_sampler_chain_default_params();
        llama_sampler* smpl = llama_sampler_chain_init(sparams);
        llama_sampler_chain_add(smpl, llama_sampler_init_top_p(0.9f, 1));
        llama_sampler_chain_add(smpl, llama_sampler_init_temp(0.75f));
        llama_sampler_chain_add(smpl, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));

        std::string generated;
        constexpr int kMaxGenTokens = 72;
        for (int step = 0; step < kMaxGenTokens && !st.stop_requested(); ++step) {
            const llama_token tok = llama_sampler_sample(smpl, impl_->ctx, -1);
            if (llama_vocab_is_eog(impl_->vocab, tok)) {
                break;
            }

            char buf[128]{};
            const int len = llama_token_to_piece(impl_->vocab, tok, buf, sizeof(buf), 0, true);
            if (len > 0) {
                generated.append(buf, static_cast<std::size_t>(len));
            }

            {
                std::scoped_lock lock(impl_->mutex);
                impl_->pending_msg = CommentatorMessage{
                    .author = "LOCAL-LLM",
                    .text = generated,
                    .timestamp = ctx.round.timestamp,
                    .is_streaming = true
                };
                impl_->has_update = true;
            }

            llama_token next_tok = tok;
            llama_batch next_batch = llama_batch_get_one(&next_tok, 1);
            if (llama_decode(impl_->ctx, next_batch) != 0) {
                break;
            }
        }

        llama_sampler_free(smpl);

        {
            std::scoped_lock lock(impl_->mutex);
            if (!generated.empty()) {
                impl_->pending_msg = CommentatorMessage{
                    .author = "LOCAL-LLM",
                    .text = generated,
                    .timestamp = ctx.round.timestamp,
                    .is_streaming = false
                };
                impl_->has_update = true;
            }
        }
        busy_.store(false, std::memory_order_release);
    });
}

std::optional<CommentatorMessage> LlamaCommentator::poll_update() {
    std::scoped_lock lock(impl_->mutex);
    if (!impl_->has_update) {
        return std::nullopt;
    }
    impl_->has_update = false;
    return impl_->pending_msg;
}

std::string LlamaCommentator::status_message() const {
    std::scoped_lock lock(impl_->mutex);
    return impl_->status;
}

} // namespace rps::ai
