// include/database/DatabaseManager.h
#pragma once

#include <soci/soci.h>
#include <string>
#include <memory>
#include <future>
#include <functional>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

struct PlayerData {
    uint64_t id;
    std::string username;
    std::string password_hash;
    int level;
    int health;
    double pos_x, pos_y, pos_z;
};

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    bool connect(const std::string& connection_string);
    void disconnect();
    
    // Sync operations (use sparingly)
    std::optional<PlayerData> getPlayerByUsername(const std::string& username);
    bool createPlayer(const PlayerData& player);
    bool updatePlayerPosition(uint64_t player_id, double x, double y, double z);
    bool updatePlayerStats(uint64_t player_id, int level, int health);
    
    // Async operations (preferred for game loop)
    std::future<std::optional<PlayerData>> getPlayerByUsernameAsync(const std::string& username);
    std::future<bool> updatePlayerPositionAsync(uint64_t player_id, double x, double y, double z);
    
    // Raw query execution
    std::vector<std::unordered_map<std::string, std::string>> executeQuery(const std::string& query);
private:
    void workerThread();
    void ensureTablesExist();
    std::unique_ptr<soci::session> sql_;
    std::string connection_string_;
    
    // Async task queue
    struct Task {
        std::function<void()> func;
    };
    
    std::queue<Task> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::thread worker_thread_;
    std::atomic<bool> worker_running_;
};