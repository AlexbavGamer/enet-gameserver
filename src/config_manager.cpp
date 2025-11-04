#include "config_manager.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include "utils/logger.h"
#include "config/constants.h"

ConfigManager::ConfigManager() {
    setDefaults();
}

void ConfigManager::setDefaults() {
    // Configuração padrão do servidor
    config_.port = 7777;
    config_.max_clients = 32;
    config_.channels = 2;
    config_.timeout_ms = 1000;
    config_.cleanup_interval_seconds = 30;
    config_.player_inactivity_timeout_minutes = 5;
    
    // Configuração padrão do banco de dados
    config_.db_connection = "db=game_db user=root host=127.0.0.1 port=3306";
    config_.db_table = "players";
    
    // Configuração padrão de scripts Lua
    config_.scripts_path = "scripts";
    
    // Configuração de desempenho
    config_.enable_binary_protocol = false;
    config_.binary_protocol_threshold = 10;
    
    // Preencher mapa de valores para acesso rápido
    config_values_["port"] = std::to_string(config_.port);
    config_values_["max_clients"] = std::to_string(config_.max_clients);
    config_values_["channels"] = std::to_string(config_.channels);
    config_values_["timeout_ms"] = std::to_string(config_.timeout_ms);
    config_values_["cleanup_interval_seconds"] = std::to_string(config_.cleanup_interval_seconds);
    config_values_["player_inactivity_timeout_minutes"] = std::to_string(config_.player_inactivity_timeout_minutes);
    config_values_["db_connection"] = config_.db_connection;
    config_values_["db_table"] = config_.db_table;
    config_values_["scripts_path"] = config_.scripts_path;
    config_values_["enable_binary_protocol"] = config_.enable_binary_protocol ? "true" : "false";
    config_values_["binary_protocol_threshold"] = std::to_string(config_.binary_protocol_threshold);
}

bool ConfigManager::loadFromFile(const std::string& filename) {
    LOG_INFO(Config::LOG_PREFIX_LUA, "Carregando configuração do arquivo: " + filename);
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Não foi possível abrir arquivo de configuração: " + filename);
        return false;
    }
    
    std::string line;
    int line_number = 0;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // Remover espaços em branco no início e no fim
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        // Ignorar linhas vazias e comentários
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // Separar chave e valor
        size_t delimiter_pos = line.find('=');
        if (delimiter_pos == std::string::npos) {
            LOG_WARNING(Config::LOG_PREFIX_LUA, "Linha " + std::to_string(line_number) + " inválida (faltando '='): " + line);
            continue;
        }
        
        std::string key = line.substr(0, delimiter_pos);
        std::string value = line.substr(delimiter_pos + 1);
        
        // Remover espaços em branco
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        // Remover aspas do valor (se existirem)
        if (!value.empty() && value[0] == '"' && value[value.length() - 1] == '"') {
            value = value.substr(1, value.length() - 2);
        }
        
        // Armazenar o valor
        config_values_[key] = value;
        
        // Atualizar a configuração estruturada
        updateConfigFromKey(key, value);
    }
    
    LOG_INFO(Config::LOG_PREFIX_LUA, "Configuração carregada com sucesso: " + std::to_string(config_values_.size()) + " valores");
    return true;
}

void ConfigManager::loadFromEnvironment() {
    LOG_INFO(Config::LOG_PREFIX_LUA, "Carregando configuração de variáveis de ambiente");
    
    // Mapeamento de variáveis de ambiente para chaves de configuração
    std::unordered_map<std::string, std::string> env_mapping = {
        {"GAME_PORT", "port"},
        {"GAME_MAX_CLIENTS", "max_clients"},
        {"GAME_CHANNELS", "channels"},
        {"GAME_TIMEOUT_MS", "timeout_ms"},
        {"GAME_CLEANUP_INTERVAL", "cleanup_interval_seconds"},
        {"GAME_INACTIVITY_TIMEOUT", "player_inactivity_timeout_minutes"},
        {"GAME_DB_CONNECTION", "db_connection"},
        {"GAME_DB_TABLE", "db_table"},
        {"GAME_SCRIPTS_PATH", "scripts_path"},
        {"GAME_BINARY_PROTOCOL", "enable_binary_protocol"},
        {"GAME_BINARY_THRESHOLD", "binary_protocol_threshold"}
    };
    
    for (const auto& [env_var, config_key] : env_mapping) {
        const char* env_value = std::getenv(env_var.c_str());
        if (env_value) {
            std::string value = env_value;
            config_values_[config_key] = value;
            updateConfigFromKey(config_key, value);
            LOG_INFO(Config::LOG_PREFIX_LUA, "Variável de ambiente definida: " + config_key + " = " + value);
        }
    }
}

void ConfigManager::updateConfigFromKey(const std::string& key, const std::string& value) {
    try {
        if (key == "port") {
            config_.port = std::stoi(value);
        } else if (key == "max_clients") {
            config_.max_clients = std::stoi(value);
        } else if (key == "channels") {
            config_.channels = std::stoi(value);
        } else if (key == "timeout_ms") {
            config_.timeout_ms = std::stoi(value);
        } else if (key == "cleanup_interval_seconds") {
            config_.cleanup_interval_seconds = std::stoi(value);
        } else if (key == "player_inactivity_timeout_minutes") {
            config_.player_inactivity_timeout_minutes = std::stoi(value);
        } else if (key == "db_connection") {
            config_.db_connection = value;
        } else if (key == "db_table") {
            config_.db_table = value;
        } else if (key == "scripts_path") {
            config_.scripts_path = value;
        } else if (key == "enable_binary_protocol") {
            config_.enable_binary_protocol = (value == "true" || value == "1");
        } else if (key == "binary_protocol_threshold") {
            config_.binary_protocol_threshold = std::stoi(value);
        }
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao converter valor para chave '" + key + "': " + std::string(e.what()));
    }
}

std::string ConfigManager::getValue(const std::string& key, const std::string& default_value) const {
    auto it = config_values_.find(key);
    if (it != config_values_.end()) {
        return it->second;
    }
    return default_value;
}

void ConfigManager::setValue(const std::string& key, const std::string& value) {
    config_values_[key] = value;
    
    // Atualizar a configuração estruturada
    updateConfigFromKey(key, value);
    
    LOG_INFO(Config::LOG_PREFIX_LUA, "Configuração atualizada: " + key + " = " + value);
}

bool ConfigManager::saveToFile(const std::string& filename) const {
    LOG_INFO(Config::LOG_PREFIX_LUA, "Salvando configuração no arquivo: " + filename);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Não foi possível criar arquivo de configuração: " + filename);
        return false;
    }
    
    // Cabeçalho do arquivo
    file << "# Arquivo de Configuração do Servidor\n";
    file << "# Gerado automaticamente pelo ConfigManager\n\n";
    
    // Seção de rede
    file << "# Configurações de Rede\n";
    file << "port = " << config_.port << "\n";
    file << "max_clients = " << config_.max_clients << "\n";
    file << "channels = " << config_.channels << "\n";
    file << "timeout_ms = " << config_.timeout_ms << "\n\n";
    
    // Seção de limpeza
    file << "# Configurações de Limpeza\n";
    file << "cleanup_interval_seconds = " << config_.cleanup_interval_seconds << "\n";
    file << "player_inactivity_timeout_minutes = " << config_.player_inactivity_timeout_minutes << "\n\n";
    
    // Seção de banco de dados
    file << "# Configurações de Banco de Dados\n";
    file << "db_connection = \"" << config_.db_connection << "\"\n";
    file << "db_table = \"" << config_.db_table << "\"\n\n";
    
    // Seção de scripts
    file << "# Configurações de Scripts Lua\n";
    file << "scripts_path = \"" << config_.scripts_path << "\"\n\n";
    
    // Seção de desempenho
    file << "# Configurações de Desempenho\n";
    file << "enable_binary_protocol = " << (config_.enable_binary_protocol ? "true" : "false") << "\n";
    file << "binary_protocol_threshold = " << config_.binary_protocol_threshold << "\n";
    
    file.close();
    LOG_INFO(Config::LOG_PREFIX_LUA, "Configuração salva com sucesso");
    return true;
}

bool ConfigManager::validate() const {
    LOG_INFO(Config::LOG_PREFIX_LUA, "Validando configuração");
    
    // Validar porta
    if (config_.port < 1 || config_.port > 65535) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Porta inválida: " + std::to_string(config_.port));
        return false;
    }
    
    // Validar número máximo de clientes
    if (config_.max_clients < 1 || config_.max_clients > 1024) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Número máximo de clientes inválido: " + std::to_string(config_.max_clients));
        return false;
    }
    
    // Validar canais
    if (config_.channels < 1 || config_.channels > 32) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Número de canais inválido: " + std::to_string(config_.channels));
        return false;
    }
    
    // Validar timeout
    if (config_.timeout_ms < 100 || config_.timeout_ms > 30000) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Timeout inválido: " + std::to_string(config_.timeout_ms));
        return false;
    }
    
    // Validar caminho de scripts
    if (config_.scripts_path.empty()) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Caminho de scripts vazio");
        return false;
    }
    
    // Validar configuração do banco de dados
    if (config_.db_connection.empty() || config_.db_table.empty()) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Configuração de banco de dados inválida");
        return false;
    }
    
    LOG_INFO(Config::LOG_PREFIX_LUA, "Configuração válida");
    return true;
}

std::vector<std::string> ConfigManager::getKeys() const {
    std::vector<std::string> keys;
    for (const auto& [key, value] : config_values_) {
        keys.push_back(key);
    }
    return keys;
}

void ConfigManager::resetToDefaults() {
    LOG_INFO(Config::LOG_PREFIX_LUA, "Resetando configuração para valores padrão");
    setDefaults();
}