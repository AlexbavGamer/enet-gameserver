#include "lua_unified.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include "encoding_utils.h"
#include "utils/logger.h"
#include "config/constants.h"

namespace fs = std::filesystem;

namespace LuaUnified {

// Instância global
std::unique_ptr<LuaUnifiedInterface> luaUnifiedInterface;

// Construtor
LuaUnifiedInterface::LuaUnifiedInterface() 
    : lua_(std::make_unique<sol::state>())
    , server_(nullptr)
    , config_manager_(nullptr)
    , database_(nullptr)
    , packet_handler_(nullptr)
    , network_manager_(nullptr)
    , game_manager_(nullptr)
    , player_manager_(nullptr)
    , db_manager_(nullptr) {
}

// Destrutor
LuaUnifiedInterface::~LuaUnifiedInterface() {
    shutdown();
}

// Inicialização
bool LuaUnifiedInterface::initialize() {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao criar estado Lua");
        return false;
    }
    
    try {
        // Abrir bibliotecas padrão
        lua_->open_libraries(sol::lib::base, sol::lib::package, sol::lib::table,
                           sol::lib::string, sol::lib::math, sol::lib::io, sol::lib::os,
                           sol::lib::debug, sol::lib::coroutine);
        
        // Registrar funções utilitárias
        registerUtilityFunctions();
        
        // Registrar todos os módulos
        registerAllModules();
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "LuaUnifiedInterface inicializado com sucesso");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao inicializar LuaUnifiedInterface: " + std::string(e.what()));
        return false;
    }
}

// Shutdown
void LuaUnifiedInterface::shutdown() {
    if (lua_) {
        lua_->collect_garbage();
        lua_.reset();
        LOG_INFO(Config::LOG_PREFIX_LUA, "LuaUnifiedInterface finalizado");
    }
}

// Carregamento de scripts
bool LuaUnifiedInterface::loadScript(const std::string& script_name, const std::string& file_path) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaUnifiedInterface não inicializado");
        return false;
    }
    
    if (!isValidScriptPath(file_path)) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Caminho de script inválido: " + file_path);
        return false;
    }
    
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Não foi possível abrir arquivo: " + file_path);
            return false;
        }
        
        std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return loadIndividualScript(script_name, code);
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao carregar script '" + script_name + "': " + std::string(e.what()));
        return false;
    }
}

bool LuaUnifiedInterface::loadScriptFromString(const std::string& script_name, const std::string& code) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaUnifiedInterface não inicializado");
        return false;
    }
    
    return loadIndividualScript(script_name, code);
}

// Carregamento de todos os scripts de um diretório
bool LuaUnifiedInterface::loadAllScripts(const std::string& scripts_directory) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaUnifiedInterface não inicializado");
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
            
            LOG_INFO(Config::LOG_PREFIX_LUA, "Carregando script: " + filename + " como " + script_name);
            
            try {
                std::ifstream file(full_path);
                if (!file.is_open()) {
                    LOG_ERROR(Config::LOG_PREFIX_LUA, "Não foi possível abrir arquivo: " + full_path);
                    failed_count++;
                    all_loaded = false;
                    continue;
                }
                
                std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                
                if (loadIndividualScript(script_name, code)) {
                    loaded_count++;
                    LOG_INFO(Config::LOG_PREFIX_LUA, "Script " + filename + " carregado com sucesso");
                } else {
                    failed_count++;
                    all_loaded = false;
                    LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao carregar script " + filename);
                }
            } catch (const std::exception& e) {
                failed_count++;
                all_loaded = false;
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Exceção ao carregar script " + filename + ": " + std::string(e.what()));
            }
        }
    }
    
    LOG_INFO(Config::LOG_PREFIX_LUA, "Carregamento de scripts concluído: " +
             std::to_string(loaded_count) + " sucesso, " + std::to_string(failed_count) + " falhas");
    
    return all_loaded;
}

// Carregamento individual de script
bool LuaUnifiedInterface::loadIndividualScript(const std::string& script_name, const std::string& content) {
    try {
        // Carregar o script
        sol::protected_function_result result = lua_->script(content, &sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao carregar script " + script_name + ": " + err.what());
            return false;
        }
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Script " + script_name + " carregado com sucesso");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Exceção ao carregar script " + script_name + ": " + std::string(e.what()));
        return false;
    }
}

// Chamada de função global
template<typename... Args>
bool LuaUnifiedInterface::callFunction(const std::string& function_name, Args... args) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaUnifiedInterface não inicializado");
        return false;
    }
    
    try {
        sol::protected_function func = lua_->get<sol::protected_function>(function_name);
        if (!func.valid()) {
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Função não encontrada: " + function_name);
            return false;
        }
        
        sol::protected_function_result result = func(args...);
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

// Chamada de função em script específico
template<typename... Args>
bool LuaUnifiedInterface::callFunctionInScript(const std::string& script_name, const std::string& function_name, Args... args) {
    if (!lua_) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "LuaUnifiedInterface não inicializado");
        return false;
    }
    
    try {
        sol::protected_function func = lua_->get<sol::protected_function>(script_name + "." + function_name);
        if (!func.valid()) {
            LOG_ERROR(Config::LOG_PREFIX_LUA, "Função não encontrada: " + script_name + "." + function_name);
            return false;
        }
        
        sol::protected_function_result result = func(args...);
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

// Verificação de scripts
bool LuaUnifiedInterface::isScriptLoaded(const std::string& script_name) const {
    if (!lua_) return false;
    
    try {
        sol::object obj = lua_->get<sol::object>(script_name);
        return obj.valid() && obj.get_type() == sol::type::table;
    } catch (...) {
        return false;
    }
}

void LuaUnifiedInterface::listLoadedScripts() const {
    LOG_INFO(Config::LOG_PREFIX_LUA, "Scripts carregados: ");
    for (auto&& [key, value] : *lua_) {
        if (value.get_type() == sol::type::table) {
            LOG_INFO(Config::LOG_PREFIX_LUA, "  - " + key.as<std::string>());
        }
    }
}

// Validação de scripts
bool LuaUnifiedInterface::isValidScriptPath(const std::string& file_path) const {
    try {
        return fs::exists(file_path) && fs::is_regular_file(file_path);
    } catch (...) {
        return false;
    }
}

// Registro de funções utilitárias
void LuaUnifiedInterface::registerUtilityFunctions() {
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
                    logLevel = Logger::Level::INFO;
                }
                
                Logger::getInstance().log(logLevel, Config::LOG_PREFIX_LUA, message);
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro na função log Lua: " + std::string(e.what()));
            }
        });

        // Função print global para Lua
        lua_->set_function("print", [this](const std::string& message) {
            LOG_INFO(Config::LOG_PREFIX_LUA, "[PRINT] " + message);
        });

        // Função safePrint global para Lua
        lua_->set_function("safePrint", [this](const std::string& message) {
            safePrint(message);
        });

        // Função para converter JSON para tabela Lua
        lua_->set_function("jsonToTable", [this](const std::string& json_str, sol::this_state s) {
            try {
                json j = json::parse(json_str);
                return sol::make_object(s, TypeConverters::convertJsonToLua(*lua_, j));
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao converter JSON para tabela: " + std::string(e.what()));
                return sol::make_object(s, sol::lua_nil);
            }
        });

        // Função para converter tabela Lua para JSON
        lua_->set_function("tableToJson", [this](sol::table table, sol::this_state s) {
            try {
                return sol::make_object(s, TypeConverters::convertLuaToJson(table).dump());
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao converter tabela para JSON: " + std::string(e.what()));
                return sol::make_object(s, sol::nil); // ✅ cria um nil válido no estado atual
            }
        });

        LOG_INFO(Config::LOG_PREFIX_LUA, "Funções utilitárias registradas com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar funções utilitárias: " + std::string(e.what()));
        throw;
    }
}

// Registro de todos os módulos
void LuaUnifiedInterface::registerAllModules() {
    registerConfigManager();
    registerSecureDatabase();
    registerSecurePacketHandler();
    registerNetworkManager();
    registerGameManager();
    registerPlayerManager();
    registerDatabaseManager();
    registerUtils();
}

// Registro do módulo ConfigManager
void LuaUnifiedInterface::registerConfigManager() {
    if (!config_manager_) return;
    
    try {
        sol::table config_module = lua_->create_table();
        
        // Funções do ConfigManager
        config_module["loadFromFile"] = [this](const std::string& filename) {
            try {
                return config_manager_->loadFromFile(filename);
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao carregar arquivo de configuração: " + std::string(e.what()));
                return false;
            }
        };
        
        config_module["loadFromEnvironment"] = [this]() {
            try {
                config_manager_->loadFromEnvironment();
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao carregar configuração de ambiente: " + std::string(e.what()));
                return false;
            }
        };
        
       config_module["getValue"] = [this](const std::string& key, sol::object default_value, sol::this_state s) -> sol::object {
            try {
                std::string default_str = "";

                if (default_value != sol::nil) {
                    if (default_value.is<std::string>()) {
                        default_str = default_value.as<std::string>();
                    } else if (default_value.is<int>()) {
                        default_str = std::to_string(default_value.as<int>());
                    } else if (default_value.is<double>()) {
                        default_str = std::to_string(default_value.as<double>());
                    } else if (default_value.is<bool>()) {
                        default_str = default_value.as<bool>() ? "true" : "false";
                    }
                }

                auto value = config_manager_->getValue(key, default_str);
                return sol::make_object(s, value); // ✅ retorno correto
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao obter valor de configuração: " + std::string(e.what()));
                return sol::make_object(s, sol::nil); // ✅ cria nil válido no mesmo estado Lua
            }
        };
        
        config_module["setValue"] = [this](const std::string& key, sol::object value) {
            try {
                std::string value_str;
                if (value.is<std::string>()) {
                    value_str = value.as<std::string>();
                } else if (value.is<int>()) {
                    value_str = std::to_string(value.as<int>());
                } else if (value.is<double>()) {
                    value_str = std::to_string(value.as<double>());
                } else if (value.is<bool>()) {
                    value_str = value.as<bool>() ? "true" : "false";
                } else {
                    value_str = "";
                }
                config_manager_->setValue(key, value_str);
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao definir valor de configuração: " + std::string(e.what()));
                return false;
            }
        };
        
        config_module["saveToFile"] = [this](const std::string& filename) {
            try {
                return config_manager_->saveToFile(filename);
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao salvar arquivo de configuração: " + std::string(e.what()));
                return false;
            }
        };
        
        config_module["validate"] = [this]() {
            try {
                return config_manager_->validate();
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao validar configuração: " + std::string(e.what()));
                return false;
            }
        };
        
        config_module["getKeys"] = [this]() {
            try {
                auto keys = config_manager_->getKeys();
                sol::table result = lua_->create_table();
                for (size_t i = 0; i < keys.size(); ++i) {
                    result[i + 1] = keys[i];
                }
                return result;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao obter chaves de configuração: " + std::string(e.what()));
                return lua_->create_table();
            }
        };
        
        config_module["resetToDefaults"] = [this]() {
            try {
                config_manager_->resetToDefaults();
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao resetar configuração: " + std::string(e.what()));
                return false;
            }
        };
        
        // Criar namespace ConfigManager
        (*lua_)["ConfigManager"] = config_module;
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Módulo ConfigManager registrado com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar módulo ConfigManager: " + std::string(e.what()));
    }
}

// Registro do módulo SecureDatabase
void LuaUnifiedInterface::registerSecureDatabase() {
    if (!database_) return;
    
    try {
        sol::table db_module = lua_->create_table();
        
        // Funções do SecureDatabase
        db_module["create"] = [this](const std::string& table, sol::table data) {
            try {
                json j = TypeConverters::convertLuaToJson(data);
                return database_->create(table, j);
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao criar registro: " + std::string(e.what()));
                return false;
            }
        };
        
        db_module["read"] = [this](const std::string& table, const std::string& where = "", sol::this_state s) {
            try {
                auto result = database_->read(table, where);
                if (result) {
                    return TypeConverters::convertJsonToLua(*lua_, *result);
                }
                return lua_->create_table();
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao ler registro: " + std::string(e.what()));
                return lua_->create_table();
            }
        };
        
        db_module["readAll"] = [this](const std::string& table, const std::string& where = "", sol::this_state s) {
            try {
                auto results = database_->readAll(table, where);
                sol::table result = lua_->create_table();
                for (size_t i = 0; i < results.size(); ++i) {
                    result[i + 1] = TypeConverters::convertJsonToLua(*lua_, results[i]);
                }
                return result;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao ler registros: " + std::string(e.what()));
                return lua_->create_table();
            }
        };
        
        db_module["update"] = [this](const std::string& table, sol::table data, const std::string& where) {
            try {
                json j = TypeConverters::convertLuaToJson(data);
                return database_->update(table, j, where);
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao atualizar registro: " + std::string(e.what()));
                return false;
            }
        };
        
        db_module["remove"] = [this](const std::string& table, const std::string& where) {
            return database_->remove(table, where);
        };
        
        // Criar namespace SecureDatabase
        (*lua_)["SecureDatabase"] = db_module;
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Módulo SecureDatabase registrado com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar módulo SecureDatabase: " + std::string(e.what()));
    }
}

// Registro do módulo SecurePacketHandler
void LuaUnifiedInterface::registerSecurePacketHandler() {
    if (!packet_handler_) return;
    
    try {
        sol::table packet_module = lua_->create_table();
        
        // Funções do SecurePacketHandler
        packet_module["createPacket"] = [this](int packet_id, sol::table data) {
            try {
                json j = TypeConverters::convertLuaToJson(data);
                auto packet = SecurePacketHandler::create(static_cast<PacketID>(packet_id), j);
                if (packet) {
                    uintptr_t packet_id_ptr = PointerManager::getInstance().registerPointer(packet.get(), "ManagedPacket");
                    return packet_id_ptr;
                }
                return static_cast<unsigned long long>(0);
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao criar pacote: " + std::string(e.what()));
                return static_cast<unsigned long long>(0);
            }
        };
        
        packet_module["sendPacket"] = [this](uintptr_t peer_id, uintptr_t packet_id, int channel = 0) {
            auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
            auto packet = PointerManager::getInstance().getPointer<ManagedPacket>(packet_id);
            
            if (peer && packet) {
                return SecurePacketHandler::sendPacket(peer, std::unique_ptr<ManagedPacket>(packet), static_cast<uint8_t>(channel));
            }
            return false;
        };
        
        packet_module["broadcastExcept"] = [this](uintptr_t sender_id, uintptr_t packet_id) {
            auto sender = PointerManager::getInstance().getPointer<ENetPeer>(sender_id);
            auto packet = PointerManager::getInstance().getPointer<ManagedPacket>(packet_id);
            
            if (sender && packet && network_manager_) {
                SecurePacketHandler::broadcastExcept(network_manager_->getServer(), sender, std::unique_ptr<ManagedPacket>(packet));
                return true;
            }
            return false;
        };
        
        packet_module["broadcastAll"] = [this](uintptr_t packet_id) {
            auto packet = PointerManager::getInstance().getPointer<ManagedPacket>(packet_id);
            
            if (packet && network_manager_) {
                SecurePacketHandler::broadcastAll(network_manager_->getServer(), std::unique_ptr<ManagedPacket>(packet));
                return true;
            }
            return false;
        };
        
        // Criar namespace SecurePacketHandler
        (*lua_)["SecurePacketHandler"] = packet_module;
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Módulo SecurePacketHandler registrado com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar módulo SecurePacketHandler: " + std::string(e.what()));
    }
}

// Registro do módulo NetworkManager
void LuaUnifiedInterface::registerNetworkManager() {
    if (!network_manager_) return;
    
    try {
        sol::table network_module = lua_->create_table();
        
        // Funções do NetworkManager
        network_module["createServer"] = [this](int port, int max_clients, int channels) {
            return network_manager_->createServer(port, max_clients, channels);
        };
        
        network_module["service"] = [this](int timeout_ms) {
            ENetEvent event;
            return network_manager_->service(&event, timeout_ms);
        };
        
        network_module["sendPacket"] = [this](uintptr_t peer_id, uintptr_t packet_id) {
            auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
            auto packet = PointerManager::getInstance().getPointer<ENetPacket>(packet_id);
            if (peer && packet) {
                NetworkManager::sendPacket(peer, std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)>(packet, enet_packet_destroy));
                return true;
            }
            return false;
        };
        
        network_module["broadcast"] = [this](uintptr_t packet_id) {
            auto packet = PointerManager::getInstance().getPointer<ENetPacket>(packet_id);
            if (packet) {
                network_manager_->broadcast(std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)>(packet, enet_packet_destroy));
                return true;
            }
            return false;
        };
        
        network_module["broadcastExcept"] = [this](uintptr_t peer_id, uintptr_t packet_id) {
            auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
            auto packet = PointerManager::getInstance().getPointer<ENetPacket>(packet_id);
            if (peer && packet) {
                network_manager_->broadcastExcept(peer, std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)>(packet, enet_packet_destroy));
                return true;
            }
            return false;
        };
        
        network_module["isInitialized"] = [this]() {
            return network_manager_->isInitialized();
        };
        
        // Criar namespace NetworkManager
        (*lua_)["NetworkManager"] = network_module;
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Módulo NetworkManager registrado com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar módulo NetworkManager: " + std::string(e.what()));
    }
}

// Registro do módulo GameManager
void LuaUnifiedInterface::registerGameManager() {
    if (!game_manager_) return;
    
    try {
        sol::table game_module = lua_->create_table();
        
        // Funções do GameManager
        game_module["handlePlayerConnect"] = [this](uintptr_t peer_id) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    game_manager_->handlePlayerConnect(peer);
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao conectar jogador");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao conectar jogador: " + std::string(e.what()));
                return false;
            }
        };
        
        game_module["handlePlayerDisconnect"] = [this](uintptr_t peer_id) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    game_manager_->handlePlayerDisconnect(peer);
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao desconectar jogador");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao desconectar jogador: " + std::string(e.what()));
                return false;
            }
        };
        
        game_module["handlePlayerMove"] = [this](uintptr_t peer_id, float x, float y) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    game_manager_->handlePlayerMove(peer, x, y);
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao mover jogador");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao mover jogador: " + std::string(e.what()));
                return false;
            }
        };
        
        game_module["handlePlayerChat"] = [this](uintptr_t peer_id, const std::string& message) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    game_manager_->handlePlayerChat(peer, message);
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao processar chat");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao processar chat: " + std::string(e.what()));
                return false;
            }
        };
        
        game_module["handlePlayerLogin"] = [this](uintptr_t peer_id, const std::string& username) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    game_manager_->handlePlayerLogin(peer, username);
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao fazer login");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao fazer login: " + std::string(e.what()));
                return false;
            }
        };
        
        game_module["processChatCommand"] = [this](int playerId, const std::string& username, const std::string& command, uintptr_t peer_id) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    bool result = game_manager_->getPlayerManager()->getDatabaseManager() != nullptr;
                    return result;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao processar comando de chat");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao processar comando de chat: " + std::string(e.what()));
                return false;
            }
        };
        
        game_module["spawnPlayerForOthers"] = [this](uintptr_t peer_id, int playerId, const std::string& username, float x, float y) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    // Simular spawn do jogador para outros
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao spawnar jogador");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao spawnar jogador: " + std::string(e.what()));
                return false;
            }
        };
        
        game_module["spawnExistingPlayersForNewPlayer"] = [this](uintptr_t peer_id) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    // Simular spawn de jogadores existentes para novo jogador
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao spawnar jogadores existentes");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao spawnar jogadores existentes: " + std::string(e.what()));
                return false;
            }
        };
        
        game_module["despawnPlayerForOthers"] = [this](uintptr_t peer_id, int playerId) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    // Simular despawn do jogador para outros
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao despawnar jogador");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao despawnar jogador: " + std::string(e.what()));
                return false;
            }
        };
        
        // Criar namespace GameManager
        (*lua_)["GameManager"] = game_module;
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Módulo GameManager registrado com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar módulo GameManager: " + std::string(e.what()));
    }
}

// Registro do módulo PlayerManager
void LuaUnifiedInterface::registerPlayerManager() {
    if (!player_manager_) return;
    
    try {
        sol::table player_module = lua_->create_table();
        
        // Funções do PlayerManager
        player_module["addPlayer"] = [this](uintptr_t peer_id, const std::string& username) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    int player_id = player_manager_->addPlayer(peer, username);
                    return player_id > 0 ? player_id : 0;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao adicionar jogador");
                return 0;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao adicionar jogador: " + std::string(e.what()));
                return 0;
            }
        };
        
        player_module["removePlayer"] = [this](uintptr_t peer_id) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    player_manager_->removePlayer(peer);
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao remover jogador");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao remover jogador: " + std::string(e.what()));
                return false;
            }
        };
        
        player_module["updatePosition"] = [this](uintptr_t peer_id, float x, float y) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    player_manager_->updatePosition(peer, x, y);
                    return true;
                }
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Peer inválido ao atualizar posição");
                return false;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao atualizar posição: " + std::string(e.what()));
                return false;
            }
        };
        
        player_module["getPlayer"] = [this](uintptr_t peer_id, sol::this_state s) {
            try {
                auto peer = PointerManager::getInstance().getPointer<ENetPeer>(peer_id);
                if (peer) {
                    auto player = player_manager_->getPlayer(peer);
                    if (player) {
                        sol::table player_table = lua_->create_table();
                        player_table["id"] = player->id;
                        player_table["username"] = player->username;
                        player_table["x"] = player->x;
                        player_table["y"] = player->y;
                        auto time_point = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            player->last_activity - std::chrono::steady_clock::now() + std::chrono::system_clock::now());
                        player_table["last_activity"] = std::chrono::system_clock::to_time_t(time_point);
                        return player_table;
                    }
                }
                return lua_->create_table();
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao obter jogador: " + std::string(e.what()));
                return lua_->create_table();
            }
        };
        
        player_module["getAllPlayers"] = [this]() {
            try {
                auto players = player_manager_->getAllPlayers();
                return TypeConverters::convertPlayerMapToLua(*lua_, players);
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao obter todos os jogadores: " + std::string(e.what()));
                return lua_->create_table();
            }
        };
        
        player_module["getPlayerCount"] = [this]() {
            try {
                return player_manager_->getPlayerCount();
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao obter contagem de jogadores: " + std::string(e.what()));
                return 0;
            }
        };
        
        player_module["broadcastMessage"] = [this](const std::string& message) {
            try {
                player_manager_->broadcastMessage(message);
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao enviar mensagem broadcast: " + std::string(e.what()));
                return false;
            }
        };
        
        player_module["cleanupInactivePlayers"] = [this](int timeout_minutes) {
            try {
                auto timeout = std::chrono::minutes(timeout_minutes);
                player_manager_->cleanupInactivePlayers(timeout);
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao limpar jogadores inativos: " + std::string(e.what()));
                return false;
            }
        };
        
        player_module["generateNextPlayerId"] = [this]() {
            try {
                return player_manager_->getPlayerCount() + 1;
            } catch (const std::exception& e) {
                LOG_ERROR(Config::LOG_PREFIX_LUA, "Erro ao gerar próximo ID de jogador: " + std::string(e.what()));
                return 1;
            }
        };
        
        // Criar namespace PlayerManager
        (*lua_)["PlayerManager"] = player_module;
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Módulo PlayerManager registrado com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar módulo PlayerManager: " + std::string(e.what()));
    }
}

// Registro do módulo DatabaseManager
void LuaUnifiedInterface::registerDatabaseManager() {
    if (!db_manager_) return;
    
    try {
        sol::table db_module = lua_->create_table();
        
        // Funções do DatabaseManager
        db_module["createPlayer"] = [this](const std::string& username, float x, float y) {
            return db_manager_->createPlayer(username, x, y);
        };
        
        db_module["updatePlayerPosition"] = [this](int player_id, float x, float y) {
            return db_manager_->updatePlayerPosition(player_id, x, y);
        };
        
        db_module["removePlayer"] = [this](int player_id) {
            return db_manager_->removePlayer(player_id);
        };
        
        db_module["isValid"] = [this]() {
            return db_manager_->isValid();
        };
        
        // Criar namespace DatabaseManager
        (*lua_)["DatabaseManager"] = db_module;
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Módulo DatabaseManager registrado com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar módulo DatabaseManager: " + std::string(e.what()));
    }
}

// Registro do módulo Utils
void LuaUnifiedInterface::registerUtils() {
    try {
        sol::table utils_module = lua_->create_table();
        
        // Funções do Utils
        utils_module["safePrint"] = [this](const std::string& message) {
            safePrint(message);
            return true;
        };
        
        utils_module["clearScreen"] = [this]() {
            ConsoleUtils::clearScreen();
            return true;
        };
        
        utils_module["pause"] = [this]() {
            ConsoleUtils::pause();
            return true;
        };
        
        utils_module["formatLogMessage"] = [this](const std::string& message) {
            return ConsoleUtils::formatLogMessage(message);
        };
        
        utils_module["toLowercase"] = [this](const std::string& str) {
            return ConsoleUtils::toLowercase(str);
        };
        
        utils_module["startsWith"] = [this](const std::string& str, const std::string& prefix) {
            return ConsoleUtils::startsWith(str, prefix);
        };
        
        utils_module["endsWith"] = [this](const std::string& str, const std::string& suffix) {
            return ConsoleUtils::endsWith(str, suffix);
        };
        
        // Criar namespace Utils
        (*lua_)["Utils"] = utils_module;
        
        LOG_INFO(Config::LOG_PREFIX_LUA, "Módulo Utils registrado com sucesso");
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao registrar módulo Utils: " + std::string(e.what()));
    }
}

// Funções de compatibilidade
bool initLua() {
    try {
        luaUnifiedInterface = std::make_unique<LuaUnifiedInterface>();
        return luaUnifiedInterface->initialize();
    } catch (const std::exception& e) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Falha ao inicializar Lua: " + std::string(e.what()));
        return false;
    }
}

void shutdownLua() {
    if (luaUnifiedInterface) {
        luaUnifiedInterface->shutdown();
        luaUnifiedInterface.reset();
    }
}

bool loadLuaScript(const std::string& script_name, const std::string& file_path) {
    if (!luaUnifiedInterface) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Sistema Lua não inicializado");
        return false;
    }
    
    return luaUnifiedInterface->loadScript(script_name, file_path);
}

template<typename... Args>
bool callLuaFunction(const std::string& script_name, const std::string& function_name, Args... args) {
    if (!luaUnifiedInterface) {
        LOG_ERROR(Config::LOG_PREFIX_LUA, "Sistema Lua não inicializado");
        return false;
    }
    
    return luaUnifiedInterface->callFunctionInScript(script_name, function_name, args...);
}

// Instanciação explícita dos templates
template bool LuaUnifiedInterface::callFunction<>(const std::string&, const std::string&);
template bool LuaUnifiedInterface::callFunction<>(const std::string&, const std::string&, const std::string&);
template bool LuaUnifiedInterface::callFunction<>(const std::string&, const std::string&, int);
template bool LuaUnifiedInterface::callFunction<>(const std::string&, const std::string&, float);
template bool LuaUnifiedInterface::callFunction<>(const std::string&, const std::string&, const std::string&, int);
template bool LuaUnifiedInterface::callFunction<>(const std::string&, const std::string&, const std::string&, float, float);

template bool LuaUnifiedInterface::callFunctionInScript<>(const std::string&, const std::string&);
template bool LuaUnifiedInterface::callFunctionInScript<>(const std::string&, const std::string&, const std::string&);
template bool LuaUnifiedInterface::callFunctionInScript<>(const std::string&, const std::string&, int);
template bool LuaUnifiedInterface::callFunctionInScript<>(const std::string&, const std::string&, float);
template bool LuaUnifiedInterface::callFunctionInScript<>(const std::string&, const std::string&, const std::string&, int);
template bool LuaUnifiedInterface::callFunctionInScript<>(const std::string&, const std::string&, const std::string&, float, float);

template bool callLuaFunction<>(const std::string&, const std::string&);
template bool callLuaFunction<>(const std::string&, const std::string&, const std::string&);
template bool callLuaFunction<>(const std::string&, const std::string&, int);
template bool callLuaFunction<>(const std::string&, const std::string&, float);
template bool callLuaFunction<>(const std::string&, const std::string&, const std::string&, int);
template bool callLuaFunction<>(const std::string&, const std::string&, const std::string&, float, float);

} // namespace LuaUnified