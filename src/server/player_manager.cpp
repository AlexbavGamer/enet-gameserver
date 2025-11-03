#include "player_manager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

PlayerManager::PlayerManager(std::unique_ptr<DatabaseManager> db_manager)
    : db_manager_(std::move(db_manager)) {
}

int PlayerManager::addPlayer(ENetPeer* peer, const std::string& username) {
    Player& player = players_[peer];
    player.id = generateNextPlayerId();
    player.username = username;
    player.last_activity = std::chrono::steady_clock::now();
    
    // Salvar no banco de dados
    if (db_manager_ && db_manager_->isValid()) {
        if (db_manager_->createPlayer(username, player.x, player.y)) {
            LOG_INFO(Config::LOG_PREFIX_PLAYER, "Jogador " + username + " (ID: " + std::to_string(player.id) + ") salvo no banco de dados");
        } else {
            LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao salvar jogador " + username + " no banco de dados");
        }
    }
    
    return player.id;
}

void PlayerManager::removePlayer(ENetPeer* peer) {
    auto it = players_.find(peer);
    if (it != players_.end()) {
        int id = it->second.id;
        LOG_INFO(Config::LOG_PREFIX_PLAYER, "Jogador " + std::to_string(id) + " desconectado");
        
        // Remover do banco de dados
        if (db_manager_ && db_manager_->isValid()) {
            db_manager_->removePlayer(id);
        }
        
        players_.erase(it);
    }
}

void PlayerManager::updatePosition(ENetPeer* peer, float x, float y) {
    auto it = players_.find(peer);
    if (it != players_.end()) {
        it->second.x = x;
        it->second.y = y;
        it->second.last_activity = std::chrono::steady_clock::now();
        
        // Atualizar no banco de dados
        if (db_manager_ && db_manager_->isValid()) {
            db_manager_->updatePlayerPosition(it->second.id, x, y);
        }
    }
}

Player* PlayerManager::getPlayer(ENetPeer* peer) {
    auto it = players_.find(peer);
    return it != players_.end() ? &it->second : nullptr;
}

const std::unordered_map<ENetPeer*, Player>& PlayerManager::getAllPlayers() const {
    return players_;
}

void PlayerManager::cleanupInactivePlayers(std::chrono::steady_clock::duration timeout) {
    auto now = std::chrono::steady_clock::now();
    auto it = players_.begin();
    
    while (it != players_.end()) {
        if (now - it->second.last_activity > timeout) {
            LOG_INFO(Config::LOG_PREFIX_PLAYER, "Removendo jogador inativo: " + it->second.username);
            it = players_.erase(it);
        } else {
            ++it;
        }
    }
}

int PlayerManager::generateNextPlayerId() {
    return next_player_id_++;
}

void PlayerManager::broadcastMessage(const std::string& message) {
    for (auto& [peer, player] : players_) {
        // Aqui você enviaria a mensagem para o cliente via ENet
        // Por enquanto, apenas logamos a mensagem
        LOG_INFO(Config::LOG_PREFIX_PLAYER, "Broadcast para jogador " + player.username + ": " + message);
    }
}