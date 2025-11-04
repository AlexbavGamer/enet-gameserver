#include "lua_manager.h"
#include "../server/server.h"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Instância global removida para evitar conflitos com lua_interface.cpp
// A implementação original está em lua_interface.cpp

LuaManager::LuaManager(Server* server) : lua_(std::make_unique<sol::state>()), server_(server) {
}

bool LuaManager::initialize() {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao criar estado Lua");
        return false;
    }
    
    try {
        // Abrir bibliotecas padrão
        lua_->open_libraries(sol::lib::base, sol::lib::package, sol::lib::table,
                           sol::lib::string, sol::lib::math, sol::lib::io, sol::lib::os);
        
        // Registrar funções Lua
        registerLuaFunctions();
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "LuaManager inicializado com sucesso");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao inicializar LuaManager: " + std::string(e.what()));
        return false;
    }
}

void LuaManager::shutdown() {
    if (lua_) {
        lua_->collect_garbage();
        lua_.reset();
        LOG_INFO(Config::LOG_PREFIX_LUA, "LuaManager finalizado");
    }
}

bool LuaManager::loadScript(const std::string& script_name, const std::string& file_path) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaManager não inicializado");
        return false;
    }
    
    if (!isValidScriptPath(file_path)) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Caminho de script inválido: " + file_path);
        return false;
    }
    
    return loadIndividualScript(script_name, file_path);
}

bool LuaManager::loadAllScripts(const std::string& scripts_directory) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaManager não inicializado");
        return false;
    }
    
    if (!fs::exists(scripts_directory) || !fs::is_directory(scripts_directory)) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Pasta de scripts não encontrada: " + scripts_directory);
        return false;
    }
    
    bool all_loaded = true;
    int loaded_count = 0;
    int failed_count = 0;
    
    for (const auto& entry : fs::directory_iterator(scripts_directory)) {
        if (entry.path().extension() == ".lua") {
            std::string filename = entry.path().filename().string();
            std::string script_name = entry.path().stem().string();
            std::string full_path = entry.path().string();
            
            LOG_DEBUG(Config::LOG_PREFIX_LUA, "Carregando script: " + filename + " como " + script_name);
            
            if (loadIndividualScript(script_name, full_path)) {
                loaded_count++;
                LOG_INFO(Config::LOG_PREFIX_LUA, "Script " + filename + " carregado com sucesso");
            } else {
                failed_count++;
                all_loaded = false;
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao carregar script " + filename);
            }
        }
    }
    
    LOG_INFO(Config::LOG_PREFIX_LUA, "Carregamento de scripts concluído: " + 
             std::to_string(loaded_count) + " sucesso, " + std::to_string(failed_count) + " falhas");
    
    return all_loaded;
}

bool LuaManager::callFunction(const std::string& function_name, sol::variadic_args args) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaManager não inicializado");
        return false;
    }
    
    try {
        sol::protected_function func = lua_->get<sol::protected_function>(function_name);
        if (!func.valid()) {
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Função não encontrada: " + function_name);
            return false;
        }
        
        sol::protected_function_result result = func(args);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao executar função " + function_name + ": " + err.what());
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Exceção ao chamar função " + function_name + ": " + e.what());
        return false;
    }
}

bool LuaManager::callFunctionInScript(const std::string& script_name, const std::string& function_name, sol::variadic_args args) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaManager não inicializado");
        return false;
    }
    
    if (!isScriptLoaded(script_name)) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Script não carregado: " + script_name);
        return false;
    }
    
    try {
        sol::protected_function func = lua_->get<sol::protected_function>(script_name + "." + function_name);
        if (!func.valid()) {
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Função não encontrada: " + script_name + "." + function_name);
            return false;
        }
        
        sol::protected_function_result result = func(args);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao executar função " + script_name + "." + function_name + ": " + err.what());
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Exceção ao chamar função " + script_name + "." + function_name + ": " + e.what());
        return false;
    }
}

bool LuaManager::isScriptLoaded(const std::string& script_name) const {
    return scripts_.find(script_name) != scripts_.end();
}

bool LuaManager::isFunctionAvailable(const std::string& script_name, const std::string& function_name) const {
    if (!isScriptLoaded(script_name)) {
        return false;
    }
    
    try {
        sol::protected_function func = lua_->get<sol::protected_function>(script_name + "." + function_name);
        return func.valid();
    } catch (...) {
        return false;
    }
}

void LuaManager::listLoadedScripts() const {
    LOG_INFO(Config::LOG_PREFIX_LUA, "Scripts carregados: ");
    for (const auto& [name, path] : scripts_) {
        LOG_INFO(Config::LOG_PREFIX_LUA, "  - " + name + " (" + path + ")");
    }
}

bool LuaManager::isValidScriptPath(const std::string& file_path) const {
    try {
        return fs::exists(file_path) && fs::is_regular_file(file_path);
    } catch (...) {
        return false;
    }
}

bool LuaManager::loadIndividualScript(const std::string& script_name, const std::string& file_path) {
    try {
        // Carregar o script
        sol::protected_function_result result = lua_->script_file(file_path, &sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao carregar script " + file_path + ": " + err.what());
            return false;
        }
        
        // Registrar script carregado
        scripts_[script_name] = file_path;
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Exceção ao carregar script " + file_path + ": " + e.what());
        return false;
    }
}

// Funções de compatibilidade removidas para evitar conflitos com lua_interface.cpp
// As implementações originais estão em lua_interface.cpp

void LuaManager::registerLuaFunctions() {
    try {
        // Função log global para Lua
        lua_->set_function("log", [this](const std::string& level, const std::string& message) {
            try {
                Logger::Level logLevel;
                if (level == "DEBUG") {
                    logLevel = Logger::Level::DEBUG;
                } else if (level == "INFO") {
                    logLevel = Logger::Level::INFO;
                } else if (level == "WARNING") {
                    logLevel = Logger::Level::WARNING;
                } else if (level == "ERROR") {
                    logLevel = Logger::Level::ERROR;
                } else {
                    // Padrão para INFO se o nível não for reconhecido
                    logLevel = Logger::Level::INFO;
                }
                
                Logger::getInstance().log(logLevel, Config::LOG_PREFIX_LUA, message);
            } catch (const std::exception& e) {
                // Se falhar o logging, tentar logar o erro em C++
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro na função log Lua: " + std::string(e.what()));
            }
        });

        lua_->set_function("getPlayers", [this]() {
            if (!server_) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Server não disponível para getPlayers");
                return 0;
            }
            return server_->getGameManager()->getPlayerManager()->getPlayerCount();
        });
        
        lua_->set_function("getPlayerCount", [this]() {
            if (!server_) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Server não disponível para getPlayerCount");
                return 0;
            }
            return server_->getGameManager()->getPlayerManager()->getPlayerCount();
        });
        
        lua_->set_function("broadcastMessage", [this](const std::string& message) {
            if (!server_) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Server não disponível para broadcastMessage");
                return false;
            }
            server_->getGameManager()->getPlayerManager()->broadcastMessage(message);
            return true;
        });
        
        lua_->set_function("getServerInfo", [this]() {
            if (!server_) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Server não disponível para getServerInfo");
                sol::table info = lua_->create_table();
                info["name"] = "Servidor Indisponível";
                info["version"] = "1.0";
                info["max_players"] = 0;
                info["port"] = 0;
                info["running"] = false;
                return info;
            }
            sol::table info = lua_->create_table();
            info["name"] = "Secure Multiplayer Server";
            info["version"] = "1.0";
            info["max_players"] = Config::MAX_CLIENTS;
            info["port"] = Config::SERVER_PORT;
            info["running"] = server_->isRunning();
            return info;
        });
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Funções Lua registradas com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar funções Lua: " + std::string(e.what()));
        throw;
    }
}