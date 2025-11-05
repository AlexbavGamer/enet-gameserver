// src/utils/PerformanceMonitor.cpp
#include "utils/PerformanceMonitor.h"
#include "utils/Logger.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

PerformanceMonitor& PerformanceMonitor::getInstance() {
    static PerformanceMonitor instance;
    return instance;
}

PerformanceMonitor::PerformanceMonitor() 
    : start_time_(std::chrono::steady_clock::now()),
      frame_time_sum_(0.0),
      frame_count_(0) {
    
    metrics_.avg_frame_time_ms = 0.0;
    metrics_.min_frame_time_ms = std::numeric_limits<double>::max();
    metrics_.max_frame_time_ms = 0.0;
    metrics_.total_frames = 0;
    metrics_.uptime_seconds = 0.0;
    metrics_.connected_players = 0;
    metrics_.total_packets_sent = 0;
    metrics_.total_packets_received = 0;
    metrics_.database_avg_query_time_ms = 0.0;
    metrics_.database_queries_executed = 0;
}

void PerformanceMonitor::startFrame() {
    frame_start_ = std::chrono::steady_clock::now();
}

void PerformanceMonitor::endFrame() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto frame_end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        frame_end - frame_start_);
    
    double frame_time_ms = duration.count() / 1000.0;
    
    frame_time_sum_ += frame_time_ms;
    frame_count_++;
    
    metrics_.min_frame_time_ms = std::min(metrics_.min_frame_time_ms, frame_time_ms);
    metrics_.max_frame_time_ms = std::max(metrics_.max_frame_time_ms, frame_time_ms);
    metrics_.total_frames++;
    
    // Atualiza média
    metrics_.avg_frame_time_ms = frame_time_sum_ / frame_count_;
    
    // Uptime
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
        frame_end - start_time_);
    metrics_.uptime_seconds = uptime.count();
}

void PerformanceMonitor::recordPacketSent() {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.total_packets_sent++;
}

void PerformanceMonitor::recordPacketReceived() {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_.total_packets_received++;
}

void PerformanceMonitor::recordDatabaseQuery(double duration_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    double total = metrics_.database_avg_query_time_ms * metrics_.database_queries_executed;
    metrics_.database_queries_executed++;
    metrics_.database_avg_query_time_ms = (total + duration_ms) / metrics_.database_queries_executed;
}

PerformanceMetrics PerformanceMonitor::getMetrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_;
}

void PerformanceMonitor::printReport() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::stringstream ss;
    ss << "\n========== Performance Report ==========\n";
    ss << "Uptime: " << std::fixed << std::setprecision(2) 
       << metrics_.uptime_seconds << " seconds\n";
    ss << "Total Frames: " << metrics_.total_frames << "\n";
    ss << "Avg Frame Time: " << std::setprecision(3) 
       << metrics_.avg_frame_time_ms << " ms\n";
    ss << "Min Frame Time: " << metrics_.min_frame_time_ms << " ms\n";
    ss << "Max Frame Time: " << metrics_.max_frame_time_ms << " ms\n";
    ss << "Avg FPS: " << std::setprecision(1) 
       << (1000.0 / metrics_.avg_frame_time_ms) << "\n";
    ss << "\nNetwork:\n";
    ss << "  Connected Players: " << metrics_.connected_players << "\n";
    ss << "  Packets Sent: " << metrics_.total_packets_sent << "\n";
    ss << "  Packets Received: " << metrics_.total_packets_received << "\n";
    ss << "\nDatabase:\n";
    ss << "  Queries Executed: " << metrics_.database_queries_executed << "\n";
    ss << "  Avg Query Time: " << std::setprecision(3) 
       << metrics_.database_avg_query_time_ms << " ms\n";
    ss << "========================================\n";
    
    Logger::info(ss.str());
}

void PerformanceMonitor::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    start_time_ = std::chrono::steady_clock::now();
    frame_time_sum_ = 0.0;
    frame_count_ = 0;
    
    metrics_ = PerformanceMetrics{};
}