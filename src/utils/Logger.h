// include/utils/Logger.h
#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

#ifdef ERROR
#undef ERROR
#endif

class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR
    };
    
    static void initialize(const std::string& filename);
    static void shutdown();
    
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    
    static void setConsoleOutput(bool enabled) { console_output_ = enabled; }
    static void setLevel(Level level) { min_level_ = level; }

private:
    static void log(Level level, const std::string& message);
    static std::string levelToString(Level level);
    static std::string getCurrentTimestamp();
    
    static std::ofstream log_file_;
    static std::mutex log_mutex_;
    static bool console_output_;
    static Level min_level_;
};