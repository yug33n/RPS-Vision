#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>

namespace rps::core {

enum class LogLevel {
    Info,
    Warn,
    Error
};

/**
 * @brief Lightweight thread-safe C++23 logger writing to console and optional log file.
 */
class Logger {
public:
    static void init(const std::filesystem::path& log_file_path) {
        std::scoped_lock lock(mutex());
        std::error_code ec;
        if (log_file_path.has_parent_path()) {
            std::filesystem::create_directories(log_file_path.parent_path(), ec);
        }
        file_stream().open(log_file_path, std::ios::out | std::ios::app);
        initialized() = file_stream().is_open();
    }

    template <typename... Args>
    static void info(std::format_string<Args...> fmt, Args&&... args) {
        write(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void warn(std::format_string<Args...> fmt, Args&&... args) {
        write(LogLevel::Warn, std::format(fmt, std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void error(std::format_string<Args...> fmt, Args&&... args) {
        write(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
    }

private:
    static constexpr std::string_view level_label(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
        }
        return "INFO";
    }

    static void write(LogLevel level, const std::string& message) {
        const auto now = std::chrono::floor<std::chrono::seconds>(
            std::chrono::system_clock::now()
        );
        const std::string line = std::format("[{:%Y-%m-%d %H:%M:%S}] [{}] {}",
                                             now, level_label(level), message);

        std::scoped_lock lock(mutex());
        std::ostream& out = (level == LogLevel::Error) ? std::cerr : std::cout;
        out << line << '\n';
        out.flush();

        if (initialized() && file_stream().is_open()) {
            file_stream() << line << '\n';
            file_stream().flush();
        }
    }

    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }

    static std::ofstream& file_stream() {
        static std::ofstream ofs;
        return ofs;
    }

    static bool& initialized() {
        static bool init_flag = false;
        return init_flag;
    }
};

} // namespace rps::core
