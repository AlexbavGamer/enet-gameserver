// include/server/Server.h
#pragma once

#include <enet/enet.h>
#include <memory>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>

class NetworkManager;
class DatabaseManager;
class LuaManager;
class World;
class Player;
class AntiCheat;

class Server {
public:
    Server(uint16_t port, size_t max_clients);
    ~Server();

    bool initialize();
    void run();
    void shutdown();

    // Getters
    NetworkManager* getNetworkManager() const { return network_manager_.get(); }
    DatabaseManager* getDatabaseManager() const { return database_manager_.get(); }
    LuaManager* getLuaManager() const { return lua_manager_.get(); }
    World* getWorld() const { return world_.get(); }

private:
    void processEvents();
    void update(float delta_time);

    void savePlayerStates();

    uint16_t port_;
    size_t max_clients_;
    std::atomic<bool> running_;
    
    std::unique_ptr<NetworkManager> network_manager_;
    std::unique_ptr<DatabaseManager> database_manager_;
    std::unique_ptr<LuaManager> lua_manager_;
    std::unique_ptr<World> world_;
    std::unique_ptr<AntiCheat> anti_cheat_;
    
    std::unordered_map<uint32_t, std::shared_ptr<Player>> players_;
    std::mutex players_mutex_;
};