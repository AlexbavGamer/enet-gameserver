#include "game_manager.h"
#include <nlohmann/json.hpp>
#include "../secure_packet_handler.h"
#include "../config/constants.h"

using json = nlohmann::json;

GameManager::GameManager(std::unique_ptr<PlayerManager> player_manager)
    : player_manager_(std::move(player_manager)) {
    // Inicializar o handler de pacotes
    packet_handler_ = std::make_unique<SecurePacketHandler>();
}

bool GameManager::initialize() {
    if (!player_manager_) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "PlayerManager não inicializado");
        return false;
    }
    
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "GameManager inicializado com sucesso");
    return true;
}

void GameManager::handlePlayerConnect(ENetPeer* peer) {
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Cliente conectado: " + std::to_string(reinterpret_cast<uintptr_t>(peer)));
    
    // Inicializar estrutura do jogador
    player_manager_->getPlayer(peer);
    
    // Enviar ping
    auto ping = createPingPacket();
    if (ping) {
        packet_handler_->sendPacket(peer, std::move(ping));
    }
}

void GameManager::handlePlayerDisconnect(ENetPeer* peer) {
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Cliente desconectado: " + std::to_string(reinterpret_cast<uintptr_t>(peer)));
    
    Player* player = player_manager_->getPlayer(peer);
    if (player) {
        LOG_INFO(Config::LOG_PREFIX_DEBUG, "Notificando desconexão do jogador: " + player->username + " (ID: " + std::to_string(player->id) + ")");
        
        // Notificar outros jogadores
        despawnPlayerForOthers(peer, player->id);
        
        // Remover jogador
        player_manager_->removePlayer(peer);
    }
}

void GameManager::handlePlayerMove(ENetPeer* peer, float x, float y) {
    if (!isValidMoveData(x, y)) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Pacote MOVE inválido!");
        return;
    }
    
    Player* player = player_manager_->getPlayer(peer);
    if (!player) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Jogador não encontrado para movimento");
        return;
    }
    
    // Atualizar posição
    player_manager_->updatePosition(peer, x, y);
    
    // Enviar movimento para outros jogadores
    json move_data = { {"id", player->id}, {"x", x}, {"y", y} };
    auto move_packet = SecurePacketHandler::create(MOVE, move_data);
    if (move_packet) {
        // Usar broadcast para todos os jogadores
        packet_handler_->broadcastAll(nullptr, std::move(move_packet));
        LOG_DEBUG(Config::LOG_PREFIX_DEBUG, "MOVE recebido: x=" + std::to_string(x) + " y=" + std::to_string(y) + " de jogador ID=" + std::to_string(player->id));
    }
}

void GameManager::handlePlayerChat(ENetPeer* peer, const std::string& message) {
    if (!isValidChatMessage(message)) {
        LOG_ERROR(Config::LOG_PREFIX_CHAT, "Pacote CHAT inválido: campo 'msg' ausente ou não é string");
        LOG_DEBUG(Config::LOG_PREFIX_CHAT, "Dados recebidos: " + message);
        return;
    }
    
    Player* player = player_manager_->getPlayer(peer);
    if (!player) {
        LOG_ERROR(Config::LOG_PREFIX_CHAT, "Jogador não encontrado para o pacote de chat");
        LOG_DEBUG(Config::LOG_PREFIX_CHAT, "Peer: " + std::to_string(reinterpret_cast<uintptr_t>(peer)));
        return;
    }
    
    LOG_INFO(Config::LOG_PREFIX_CHAT, player->username + ": " + message);
    
    // Verificar se é um comando
    if (message.starts_with("/")) {
        if (!processChatCommand(player->id, player->username, message, peer)) {
            sendChatResponse(peer, Config::LUA_ERROR_MSG);
        }
    } else {
        // Chat normal
        broadcastChatMessage(player->username, message);
    }
}

void GameManager::handlePlayerLogin(ENetPeer* peer, const std::string& username) {
    LOG_INFO(Config::LOG_PREFIX_LOGIN, "Login solicitado: " + username);
    
    // Adicionar jogador
    int playerId = player_manager_->addPlayer(peer, username);
    
    // Enviar resposta de login
    auto login_data = json{{"msg", Config::LOGIN_SUCCESS_MSG}, {"id", playerId}};
    auto login_reply = SecurePacketHandler::create(LOGIN, login_data);
    if (login_reply) {
        packet_handler_->sendPacket(peer, std::move(login_reply));
    }
    
    // Spawn para outros jogadores
    spawnPlayerForOthers(peer, playerId, username, 0, 0);
    
    // Enviar jogadores existentes para o novo jogador
    spawnExistingPlayersForNewPlayer(peer);
}

bool GameManager::processChatCommand(int playerId, const std::string& username, const std::string& command, ENetPeer* peer) {
    LOG_DEBUG(Config::LOG_PREFIX_CHAT, "Comando recebido: " + command + " (processando via Lua)");
    
    // Processar comando básico em C++
    if (command == "/ajuda") {
        sendChatResponse(peer, Config::HELP_MSG);
        return true;
    } else if (command == "/jogadores" || command == "/tempo") {
        // Comandos que poderiam ser implementados
        sendChatResponse(peer, "Comando '" + command + "' ainda não implementado.");
        return true;
    } else {
        sendChatResponse(peer, Config::UNKNOWN_COMMAND_MSG);
        return true;
    }
}

void GameManager::sendChatResponse(ENetPeer* peer, const std::string& message) {
    json response_data = { {"user", "Sistema"}, {"msg", message} };
    auto response_packet = SecurePacketHandler::create(CHAT, response_data);
    if (response_packet) {
        packet_handler_->sendPacket(peer, std::move(response_packet));
    }
}

void GameManager::broadcastChatMessage(const std::string& username, const std::string& message) {
    json chat_data = { {"user", username}, {"msg", message} };
    auto chat_packet = SecurePacketHandler::create(CHAT, chat_data);
    if (chat_packet) {
        packet_handler_->broadcastAll(nullptr, std::move(chat_packet));
    }
}

void GameManager::spawnPlayerForOthers(ENetPeer* peer, int playerId, const std::string& username, float x, float y) {
    auto spawn_packet = createSpawnPlayerPacket(playerId, username, x, y);
    if (spawn_packet) {
        packet_handler_->broadcastExcept(nullptr, peer, std::move(spawn_packet));
    }
}

void GameManager::spawnExistingPlayersForNewPlayer(ENetPeer* peer) {
    for (const auto& [peer_player, player] : player_manager_->getAllPlayers()) {
        if (peer_player != peer && player.id > 0) {
            auto other_packet = createSpawnPlayerPacket(player.id, player.username, player.x, player.y);
            if (other_packet) {
                packet_handler_->sendPacket(peer, std::move(other_packet));
            }
        }
    }
}

void GameManager::despawnPlayerForOthers(ENetPeer* peer, int playerId) {
    auto logout_packet = createLogoutPacket(playerId);
    if (logout_packet) {
        packet_handler_->broadcastExcept(nullptr, peer, std::move(logout_packet));
    }
}

bool GameManager::isValidChatMessage(const std::string& message) const {
    return !message.empty();
}

bool GameManager::isValidMoveData(float x, float y) const {
    // Validação básia - coordenadas devem ser números finitos
    return std::isfinite(x) && std::isfinite(y);
}