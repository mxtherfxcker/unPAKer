// unPAKer - Game Resource Archive Extractor
// Copyright (c) 2025 mxtherfxcker and contributors
// Licensed under MIT License

#include "logger.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef ERROR
#undef ERROR
#endif

namespace unpaker {

Logger& Logger::instance() {
    static Logger _instance;
    return _instance;
}

Logger::Logger()
    : dev_mode_(false),
              log_file_(nullptr) {
}

Logger::~Logger() {
    shutdown();
}

void Logger::initialize(bool dev_mode) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    dev_mode_ = dev_mode;

    if (dev_mode_) {
        log_file_path_ = get_log_file_path();

        try {
            if (!log_file_path_.parent_path().empty()) {
                fs::create_directories(log_file_path_.parent_path());
            }

            log_file_ = std::make_unique<std::ofstream>(
                log_file_path_,
                std::ios::app
            );

            if (log_file_ && log_file_->is_open()) {
                *log_file_ << "\n========================================\n";
                *log_file_ << "unPAKer Log Session Started\n";
                *log_file_ << "Timestamp: " << get_timestamp() << "\n";
                *log_file_ << "DEV_MODE: ENABLED\n";
                *log_file_ << "========================================\n";
                log_file_->flush();

                std::cout << "[INFO] Logger: Log file opened: " << log_file_path_.string() << std::endl;
            } else {
                std::cerr << "[WARNING] Logger: Failed to open log file: " << log_file_path_.string() << std::endl;
                log_file_.reset();
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Logger: Exception opening log file: " << e.what() << std::endl;
            log_file_.reset();
        }
    }
}

bool Logger::is_dev_mode() const {
    return dev_mode_;
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex_);

    if (dev_mode_ || level != LogLevel::DEBUG) {
        write_to_console(level, message);
    }

    if (dev_mode_ && log_file_ && log_file_->is_open()) {
        write_to_file(level, message);
    }
}

void Logger::debug(const std::string& message) {
    if (!dev_mode_) {
        return;
    }
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::success(const std::string& message) {
    log(LogLevel::SUCCESS, message);
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(log_mutex_);

    if (log_file_ && log_file_->is_open()) {
        *log_file_ << "========================================\n";
        *log_file_ << "unPAKer Log Session Ended\n";
        *log_file_ << "Timestamp: " << get_timestamp() << "\n";
        *log_file_ << "========================================\n\n";
        log_file_->flush();
        log_file_->close();
    }

    log_file_.reset();
}

std::string Logger::get_log_level_string(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::SUCCESS: return "SUCCESS";
        default:                return "UNKNOWN";
    }
}

std::string Logger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()
    ) % 1000;

    std::tm tm_buf;
    localtime_s(&tm_buf, &time_t);

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

fs::path Logger::get_log_file_path() const {
    wchar_t exe_path[MAX_PATH] = {0};
    if (GetModuleFileNameW(nullptr, exe_path, MAX_PATH) != 0) {
        fs::path exe_file(exe_path);
        fs::path exe_dir = exe_file.parent_path();

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_s(&tm_buf, &time_t);

        std::ostringstream oss;
        oss << "unPAKer_";
        oss << std::put_time(&tm_buf, "%Y-%m-%d");
        oss << ".log";

        return exe_dir / oss.str();
    }

    return fs::current_path() / "unPAKer.log";
}

void Logger::write_to_console(LogLevel level, const std::string& message) {
    std::string level_str = get_log_level_string(level);
    std::cout << "[" << level_str << "] " << message << std::endl;
}

void Logger::write_to_file(LogLevel level, const std::string& message) {
    if (!log_file_ || !log_file_->is_open()) {
        return;
    }

    std::string timestamp = get_timestamp();
    std::string level_str = get_log_level_string(level);

    *log_file_ << "[" << timestamp << "] [" << level_str << "] " << message << "\n";
    log_file_->flush();
}

bool Logger::should_filter_debug(const std::string& message) const {
    if (!dev_mode_ && message.find("[DEBUG]") == 0) {
        return true;
    }
    return false;
}

} // namespace unpaker
