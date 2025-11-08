// src/utils/Logger.cpp
#include "utils/Logger.h"

std::ofstream Logger::log_file_;
std::mutex Logger::log_mutex_;
bool Logger::console_output_ = true;
Logger::Level Logger::min_level_ = Logger::Level::DEBUG;

void Logger::initialize(const std::string& filename) {
    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_file_.open(filename, std::ios::app);
    } // 🔓 mutex liberado aqui

    if (log_file_.is_open()) {
        log(Level::INFO, "Logger initialized");
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (log_file_.is_open()) {
        log(Level::INFO, "Logger shutting down");
        log_file_.close();
    }
}

void Logger::debug(const std::string& message) {
    log(Level::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(Level::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(Level::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(Level::ERROR, message);
}

void Logger::log(Level level, const std::string& message) {
    if (level < min_level_) return;
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string timestamp = getCurrentTimestamp();
    std::string level_str = levelToString(level);
    std::string log_line = "[" + timestamp + "] [" + level_str + "] " + message;
    
    if (console_output_) {
        std::cout << log_line << std::endl;
    }
    
    if (log_file_.is_open()) {
        log_file_ << log_line << std::endl;
        log_file_.flush();
    }
}

std::string Logger::levelToString(Level level) {
    switch (level) {
        case Level::DEBUG:   return "DEBUG";
        case Level::INFO:    return "INFO";
        case Level::WARNING: return "WARNING";
        case Level::ERROR:   return "ERROR";
        default:             return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}