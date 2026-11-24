#include "ai/TemplateCommentator.hpp"

#include <algorithm>
#include <format>
#include <span>
#include <string_view>

namespace rps::ai {

namespace {

constexpr std::array<std::string_view, 5> kEnPlayerWin = {
    "Clean read! Your {} crushed my {}.",
    "Point to the human. {} beats {} fair and square.",
    "Sharp timing on that {}! My {} never stood a chance.",
    "Alright, you got me with {} this round.",
    "Solid counter-play — {} takes down {}."
};

constexpr std::array<std::string_view, 4> kEnPlayerStreak = {
    "Unstoppable! {} wins in a row with that {}!",
    "You are on fire — {} straight victories! My Markov matrix is sweating.",
    "Streak {}! Are you reading my neural weights?",
    "Dominant {}! That makes {} wins in a row."
};

constexpr std::array<std::string_view, 5> kEnAiWin = {
    "Predicted! My {} slices right through your {}.",
    "Too readable! {} beats {} every time.",
    "Markov-2 sends its regards: {} over {}.",
    "Locked in {} and took the point from your {}.",
    "Nice try with {}, but {} takes this round."
};

constexpr std::array<std::string_view, 4> kEnAiStreak = {
    "AI streak at {}! You need to mix up your pattern.",
    "{} in a row for the machine! Try breaking your rhythm.",
    "I have your {} dialed in — {} straight wins for the AI!",
    "Streak {}! My transition matrix is two steps ahead."
};

constexpr std::array<std::string_view, 4> kEnDraw = {
    "Mirror match! Both locked {} — nobody blinks.",
    "Great minds think alike: {} vs {}.",
    "Deadlock on {}! Run it back.",
    "Stalemate with {}. Break the tie next round!"
};

constexpr std::array<std::string_view, 3> kEnDrawChain = {
    "{} draws in a row! We are stuck in a quantum loop.",
    "Another {} tie! Streak of {} draws — someone switch it up!",
    "Synchronized {} again! {} consecutive stalemates."
};

constexpr std::array<std::string_view, 3> kEnRepeatMove = {
    "Bold strategy throwing {} {} times in a row...",
    "{} again?! That is {} times straight — bold or stubborn?",
    "I see you really love {}. {} consecutive throws!"
};

constexpr std::array<std::string_view, 4> kEnInvalid = {
    "Anti-cheat alert: no clear gesture detected on SHOOT!",
    "Keep your hand clearly inside the frame when the timer hits zero.",
    "Invalid gesture! Show a clean Rock, Paper, or Scissors.",
    "Nice try hiding your hand — round marked invalid."
};

constexpr std::array<std::string_view, 3> kEnCheater = {
    "Cheater mode active: I saw your {} and countered with {}!",
    "Unbeatable mode engaged — your {} never had a chance against {}.",
    "I peeked at your {} at the last millisecond. {} wins!"
};

// Localized Russian UI commentary dictionary
constexpr std::array<std::string_view, 5> kRuPlayerWin = {
    "Отличный ход! Твой {} уверенно бьёт мой {}.",
    "Очко человеку: {} сильнее, чем {}.",
    "Точный тайминг на {}! Мой {} не справился.",
    "Хорош! На этот раз {} приносит тебе победу.",
    "Грамотная контратака — {} забирает раунд у {}."
};

constexpr std::array<std::string_view, 4> kRuPlayerStreak = {
    "Неудержим! Уже {} побед подряд с ходом {}!",
    "Вот это серия — {} побед кряду! Матрица Маркова перегревается.",
    "Серия {}! Ты что, читаешь мои веса?",
    "Доминация через {}! Это уже {} победа подряд."
};

constexpr std::array<std::string_view, 5> kRuAiWin = {
    "Прочитано! Мой {} легко проходит твой {}.",
    "Слишком предсказуемо: {} побеждает {}.",
    "Цепь Маркова сработала: {} против {}.",
    "Я выбрал {} и забрал очко у твоего {}.",
    "Хорошая попытка с {}, но {} забирает этот раунд."
};

constexpr std::array<std::string_view, 4> kRuAiStreak = {
    "Серия ИИ: {}! Пора менять тактику.",
    "{} побед подряд за машиной! Попробуй сбить ритм.",
    "Я полностью просчитал твой {} — уже {} победы ИИ подряд!",
    "Серия {}! Матрица переходов на два шага впереди."
};

constexpr std::array<std::string_view, 4> kRuDraw = {
    "Зеркалка! Оба показали {} — ничья.",
    "Мыслим одинаково: {} против {}.",
    "Полный паритет на {}! Погнали следующий раунд.",
    "Ничья на {}. Кто-то должен уступить!"
};

constexpr std::array<std::string_view, 3> kRuDrawChain = {
    "{} ничьих подряд! Мы застряли в одной петле.",
    "Снова {}! Уже {} ничьих кряду — меняй жест!",
    "Синхронный {}! {} ничьих подряд."
};

constexpr std::array<std::string_view, 3> kRuRepeatMove = {
    "Смелая тактика — кидать {} уже {} раз подряд...",
    "Снова {}?! Это уже {} раз кряду!",
    "Вижу, тебе очень нравится {}. {} повторов подряд!"
};

constexpr std::array<std::string_view, 4> kRuInvalid = {
    "Античит: жест не распознан в момент SHOOT!",
    "Держи руку чётко в кадре, когда таймер доходит до нуля.",
    "Неверный жест! Покажи чёткий Камень, Ножницы или Бумагу.",
    "Хитрить не выйдет — раунд не засчитан."
};

constexpr std::array<std::string_view, 3> kRuCheater = {
    "Режим читера: я увидел твой {} и мгновенно ответил {}!",
    "Непобедимый режим активен — против {} у твоего {} не было шансов.",
    "Я подсмотрел твой {} на последнем кадре. {} побеждает!"
};

std::string_view localized_move(game::Move m, Language lang) noexcept {
    if (lang == Language::Ru) {
        switch (m) {
            case game::Move::Rock:     return "Камень";
            case game::Move::Paper:    return "Бумага";
            case game::Move::Scissors: return "Ножницы";
            case game::Move::None:     return "Нет жеста";
        }
    }
    return game::to_string(m);
}

std::string pick_non_repeating(
    std::span<const std::string_view> bank,
    int& last_idx,
    std::mt19937& rng
) {
    if (bank.empty()) {
        return "";
    }
    int idx = 0;
    if (bank.size() > 1) {
        std::uniform_int_distribution<int> dist(0, static_cast<int>(bank.size()) - 1);
        do {
            idx = dist(rng);
        } while (idx == last_idx);
    }
    last_idx = idx;
    return std::string(bank[static_cast<std::size_t>(idx)]);
}

std::string format_two_args(std::string_view tmpl, std::string_view a, std::string_view b) {
    std::string out(tmpl);
    if (const auto pos1 = out.find("{}"); pos1 != std::string::npos) {
        out.replace(pos1, 2, a);
        if (const auto pos2 = out.find("{}", pos1 + a.size()); pos2 != std::string::npos) {
            out.replace(pos2, 2, b);
        }
    }
    return out;
}

} // namespace

TemplateCommentator::TemplateCommentator()
    : rng_(std::random_device{}()) {
    last_index_.fill(-1);
}

void TemplateCommentator::request_commentary(const CommentaryContext& ctx) {
    pending_ = CommentatorMessage{
        .author = "CYBER-CASTER",
        .text = generate_sync(ctx),
        .timestamp = ctx.round.timestamp,
        .is_streaming = false
    };
}

std::optional<CommentatorMessage> TemplateCommentator::poll_update() {
    auto out = std::move(pending_);
    pending_.reset();
    return out;
}

TemplateCommentator::Category TemplateCommentator::classify_context(const CommentaryContext& ctx) const noexcept {
    if (ctx.round.result == game::RoundResult::InvalidMove) {
        return Category::InvalidMove;
    }
    if (ctx.round.difficulty == game::AiDifficulty::Cheater &&
        ctx.round.result == game::RoundResult::AiWin) {
        return Category::CheaterTaunt;
    }
    if (ctx.round.result == game::RoundResult::Draw) {
        return (ctx.consecutive_draws >= 2) ? Category::DrawChain : Category::Draw;
    }
    if (ctx.consecutive_same_player_move >= 3 && ctx.round.result == game::RoundResult::AiWin) {
        return Category::RepeatMove;
    }
    if (ctx.round.result == game::RoundResult::PlayerWin) {
        return (ctx.stats.current_streak >= 3) ? Category::PlayerStreak : Category::PlayerWin;
    }
    return (ctx.stats.current_streak <= -3) ? Category::AiStreak : Category::AiWin;
}

std::string TemplateCommentator::generate_sync(const CommentaryContext& ctx) {
    const Category cat = classify_context(ctx);
    int& last_idx = last_index_[static_cast<std::size_t>(cat)];
    const bool ru = (ctx.language == Language::Ru);

    const std::string_view p_move = localized_move(ctx.round.player_move, ctx.language);
    const std::string_view a_move = localized_move(ctx.round.ai_move, ctx.language);
    const std::string streak_str = std::to_string(std::abs(ctx.stats.current_streak));
    const std::string draws_str = std::to_string(ctx.consecutive_draws);
    const std::string repeat_str = std::to_string(ctx.consecutive_same_player_move);

    switch (cat) {
        case Category::PlayerWin:
            return format_two_args(
                pick_non_repeating(ru ? std::span<const std::string_view>(kRuPlayerWin) : kEnPlayerWin, last_idx, rng_),
                p_move, a_move);
        case Category::PlayerStreak:
            return format_two_args(
                pick_non_repeating(ru ? std::span<const std::string_view>(kRuPlayerStreak) : kEnPlayerStreak, last_idx, rng_),
                streak_str, p_move);
        case Category::AiWin:
            return format_two_args(
                pick_non_repeating(ru ? std::span<const std::string_view>(kRuAiWin) : kEnAiWin, last_idx, rng_),
                a_move, p_move);
        case Category::AiStreak:
            return format_two_args(
                pick_non_repeating(ru ? std::span<const std::string_view>(kRuAiStreak) : kEnAiStreak, last_idx, rng_),
                streak_str, p_move);
        case Category::Draw:
            return format_two_args(
                pick_non_repeating(ru ? std::span<const std::string_view>(kRuDraw) : kEnDraw, last_idx, rng_),
                p_move, a_move);
        case Category::DrawChain:
            return format_two_args(
                pick_non_repeating(ru ? std::span<const std::string_view>(kRuDrawChain) : kEnDrawChain, last_idx, rng_),
                draws_str, p_move);
        case Category::RepeatMove:
            return format_two_args(
                pick_non_repeating(ru ? std::span<const std::string_view>(kRuRepeatMove) : kEnRepeatMove, last_idx, rng_),
                p_move, repeat_str);
        case Category::InvalidMove:
            return pick_non_repeating(ru ? std::span<const std::string_view>(kRuInvalid) : kEnInvalid, last_idx, rng_);
        case Category::CheaterTaunt:
            return format_two_args(
                pick_non_repeating(ru ? std::span<const std::string_view>(kRuCheater) : kEnCheater, last_idx, rng_),
                p_move, a_move);
        default:
            return "";
    }
}

} // namespace rps::ai
