#ifndef PLAYER_MANAGER_H
#define PLAYER_MANAGER_H

#include "../config/constants.h"
#include "../utils/logger.h"
#include "../database/database_manager.h"
#include <enet/enet.h>
#include <unordered_map>
#include <memory>
#include <chrono>

// Estrutura do Jogador
struct Player {
    int id = 0;
    std::string username = "unknown";
    float x = 0, y = 0;
    std::chrono::steady_clock::time_point last_activity;
    
    Player() : last_activity(std::chrono::steady_clock::now()) {}
};

class PlayerManager {
private:
    std::unordered_map<ENetPeer*, Player> players_;
    int next_player_id_ = 1;
    std::unique_ptr<DatabaseManager> db_manager_;
    
public:
    PlayerManager(std::unique_ptr<DatabaseManager> db_manager);
    ~PlayerManager() = default;
    
    // Gerenciamento de jogadores
    int addPlayer(ENetPeer* peer, const std::string& username);
    void removePlayer(ENetPeer* peer);
    void updatePosition(ENetPeer* peer, float x, float y);
    
    // Acesso a jogadores
    Player* getPlayer(ENetPeer* peer);
    const std::unordered_map<ENetPeer*, Player>& getAllPlayers() const;
    
    // Manutenção
    void cleanupInactivePlayers(std::chrono::steady_clock::duration timeout);
    
    // Métodos de acesso
    DatabaseManager* getDatabaseManager() const { return db_manager_.get(); }
    
    // Métodos de contagem e broadcast
    int getPlayerCount() const { return static_cast<int>(players_.size()); }
    void broadcastMessage(const std::string& message);
    
private:
    // Gera próximo ID de jogador
    int generateNextPlayerId();
};

#endif // PLAYER_MANAGER_H