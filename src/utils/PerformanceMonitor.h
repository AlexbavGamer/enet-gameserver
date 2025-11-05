// include/utils/PerformanceMonitor.h
#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <mutex>

struct PerformanceMetrics {
    double avg_frame_time_ms;
    double min_frame_time_ms;
    double max_frame_time_ms;
    size_t total_frames;
    double uptime_seconds;
    
    size_t connected_players;
    size_t total_packets_sent;
    size_t total_packets_received;
    
    double database_avg_query_time_ms;
    size_t database_queries_executed;
};

class PerformanceMonitor {
public:
    static PerformanceMonitor& getInstance();
    
    void startFrame();
    void endFrame();
    
    void recordPacketSent();
    void recordPacketReceived();
    
    void recordDatabaseQuery(double duration_ms);
    
    void setConnectedPlayers(size_t count) { metrics_.connected_players = count; }
    
    PerformanceMetrics getMetrics() const;
    void printReport() const;
    
    void reset();

private:
    PerformanceMonitor();
    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;
    
    mutable std::mutex mutex_;
    
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point frame_start_;
    
    PerformanceMetrics metrics_;
    
    double frame_time_sum_;
    size_t frame_count_;
};