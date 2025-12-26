// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2026 mxtherfxcker and contributors
// Licensed under MIT License

#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <filesystem>

#ifdef ERROR
#undef ERROR
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef SUCCESS
#undef SUCCESS
#endif

namespace fs = std::filesystem;

namespace unpaker {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    SUCCESS = 4
};

enum class ConsoleColor {
    DEFAULT = 7,    // White
    BLUE = 9,       // Bright Blue
    GREEN = 10,     // Bright Green
    CYAN = 11,      // Bright Cyan
    RED = 12,       // Bright Red
    MAGENTA = 13,   // Bright Magenta
    YELLOW = 14,    // Bright Yellow
    WHITE = 15      // Bright White
};

class Logger {
public:
    static Logger& instance();

    void initialize(bool dev_mode);

    bool is_dev_mode() const;

    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void success(const std::string& message);

    void shutdown();

    bool should_filter_debug(const std::string& message) const;
    void set_colors_enabled(bool enabled) { colors_enabled_ = enabled; }
    bool are_colors_enabled() const { return colors_enabled_; }

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    bool dev_mode_;
    bool colors_enabled_;
    std::unique_ptr<std::ofstream> log_file_;
    std::mutex log_mutex_;
    fs::path log_file_path_;

    std::string get_log_level_string(LogLevel level) const;
    std::string get_timestamp() const;
    fs::path get_log_file_path() const;
    void write_to_console(LogLevel level, const std::string& message);
    void write_to_file(LogLevel level, const std::string& message);
    void set_console_color(ConsoleColor color);
    void reset_console_color();
    std::string get_ansi_color_code(LogLevel level) const;
    ConsoleColor get_color_for_level(LogLevel level) const;
};

#ifdef _DEBUG
    #define LOG_DEBUG(msg) unpaker::Logger::instance().debug(msg)
#else
    #define LOG_DEBUG(msg) ((void)0)
#endif

#define LOG_INFO(msg) unpaker::Logger::instance().info(msg)
#define LOG_WARNING(msg) unpaker::Logger::instance().warning(msg)
#define LOG_ERROR(msg) unpaker::Logger::instance().error(msg)
#define LOG_SUCCESS(msg) unpaker::Logger::instance().success(msg)

#ifdef _DEBUG
    #define DEBUG_COUT(expr) do { std::cout << expr; } while(0)
    #define DEBUG_CERR(expr) do { std::cerr << expr; } while(0)
#else
    #define DEBUG_COUT(expr) ((void)0)
    #define DEBUG_CERR(expr) ((void)0)
#endif

inline void debug_cout(const std::string& message) {
    auto& logger = unpaker::Logger::instance();
    if (logger.is_dev_mode()) {
        logger.debug(message);
    }
}

} // namespace unpaker
