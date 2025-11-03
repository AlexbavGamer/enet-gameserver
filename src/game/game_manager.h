#ifndef GAME_MANAGER_H
#define GAME_MANAGER_H

#include "../config/constants.h"
#include "../utils/logger.h"
#include "../server/player_manager.h"
#include "../secure_packet_handler.h"
#include <enet/enet.h>
#include <memory>

class GameManager {
private:
    std::unique_ptr<PlayerManager> player_manager_;
    std::unique_ptr<SecurePacketHandler> packet_handler_;
    
public:
    GameManager(std::unique_ptr<PlayerManager> player_manager);
    ~GameManager() = default;
    
    // Inicialização
    bool initialize();
    
    // Handlers de eventos de jogo
    void handlePlayerConnect(ENetPeer* peer);
    void handlePlayerDisconnect(ENetPeer* peer);
    void handlePlayerMove(ENetPeer* peer, float x, float y);
    void handlePlayerChat(ENetPeer* peer, const std::string& message);
    void handlePlayerLogin(ENetPeer* peer, const std::string& username);
    
    // Métodos de acesso
    PlayerManager* getPlayerManager() const { return player_manager_.get(); }
    
private:
    // Métodos auxiliares para chat
    bool processChatCommand(int playerId, const std::string& username, const std::string& command, ENetPeer* peer);
    void sendChatResponse(ENetPeer* peer, const std::string& message);
    void broadcastChatMessage(const std::string& username, const std::string& message);
    
    // Métodos auxiliares para spawn/despawn
    void spawnPlayerForOthers(ENetPeer* peer, int playerId, const std::string& username, float x, float y);
    void spawnExistingPlayersForNewPlayer(ENetPeer* peer);
    void despawnPlayerForOthers(ENetPeer* peer, int playerId);
    
    // Validação de dados
    bool isValidChatMessage(const std::string& message) const;
    bool isValidMoveData(float x, float y) const;
};

#endif // GAME_MANAGER_H