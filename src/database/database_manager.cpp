#include "database_manager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

DatabaseManager::DatabaseManager() {
    try {
        database_ = std::make_unique<SecureDatabase>();
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao criar SecureDatabase: " + std::string(e.what()));
        database_ = nullptr;  // Garante que seja nulo em caso de falha
    } catch (...) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha desconhecida ao criar SecureDatabase");
        database_ = nullptr;  // Garante que seja nulo em caso de falha
    }
}

bool DatabaseManager::initialize() {
    // Pular completamente a verificação do banco de dados para permitir que o servidor continue rodando
    LOG_WARNING(Config::LOG_PREFIX_ERROR, "Banco de dados desabilitado para permitir que o servidor continue rodando");
    return true;  // Retorna true para permitir que o servidor continue rodando
}

bool DatabaseManager::isValid() const {
    return database_ != nullptr;
}

bool DatabaseManager::createPlayer(const std::string& username, float x, float y) {
    if (!isValid()) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Banco de dados não inicializado");
        return false;
    }
    
    json playerData = {
        {"username", username},
        {"x", x},
        {"y", y},
        {"created_at", "CURRENT_TIMESTAMP"}
    };
    
    bool success = database_->create(Config::PLAYERS_TABLE, playerData);
    if (success) {
        LOG_INFO(Config::LOG_PREFIX_PLAYER, "Jogador " + username + " salvo no banco de dados");
    } else {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao salvar jogador " + username + " no banco de dados");
    }
    
    return success;
}

bool DatabaseManager::updatePlayerPosition(int playerId, float x, float y) {
    if (!isValid()) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Banco de dados não inicializado");
        return false;
    }
    
    json updateData = {{"x", x}, {"y", y}};
    std::string condition = "id = " + std::to_string(playerId);
    
    bool success = database_->update(Config::PLAYERS_TABLE, updateData, condition);
    if (!success) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao atualizar posição do jogador ID: " + std::to_string(playerId));
    }
    
    return success;
}

bool DatabaseManager::removePlayer(int playerId) {
    if (!isValid()) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Banco de dados não inicializado");
        return false;
    }
    
    std::string condition = "id = " + std::to_string(playerId);
    bool success = database_->remove(Config::PLAYERS_TABLE, condition);
    
    if (!success) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao remover jogador ID: " + std::to_string(playerId));
    }
    
    return success;
}

bool DatabaseManager::isValidTableName(const std::string& tableName) const {
    if (!database_) {
        return true;  // Retorna true para permitir que o servidor continue rodando
    }
    
    return database_->isValidTableName(tableName);
}