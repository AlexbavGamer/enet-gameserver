#pragma once
#include <sol/sol.hpp>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>
#include <any>
#include <nlohmann/json.hpp>
#include <enet/enet.h>
#include "server_common.h"
#include "config_manager.h"
#include "secure_database.h"
#include "network/network_manager.h"
#include "game/game_manager.h"
#include "server/player_manager.h"
#include "database/database_manager.h"
#include "utils/console_utils.h"

// Forward declarations para evitar dependência circular
class Server;

using json = nlohmann::json;

// Forward declarations
class Server;
class NetworkManager;
class GameManager;
class PlayerManager;
class DatabaseManager;
class ConfigManager;
class SecureDatabase;
class SecurePacketHandler;

namespace LuaUnified {

// Gerenciador de ponteiros para conversão C++ ↔ Lua
class PointerManager {
private:
    std::unordered_map<uintptr_t, std::string> pointer_registry_;
    std::unordered_map<std::string, uintptr_t> id_registry_;
    uintptr_t next_id_ = 1;

public:
    static PointerManager& getInstance() {
        static PointerManager instance;
        return instance;
    }

    template<typename T>
    uintptr_t registerPointer(T* ptr, const std::string& type) {
        if (!ptr) return 0;
        uintptr_t key = reinterpret_cast<uintptr_t>(ptr);
        pointer_registry_[key] = type;
        id_registry_[type + "_" + std::to_string(key)] = key;
        return key;
    }

    template<typename T>
    T* getPointer(uintptr_t id) {
        auto it = pointer_registry_.find(id);
        if (it != pointer_registry_.end()) {
            return reinterpret_cast<T*>(id);
        }
        return nullptr;
    }

    std::string getPointerType(uintptr_t id) {
        auto it = pointer_registry_.find(id);
        return it != pointer_registry_.end() ? it->second : "";
    }

    uintptr_t getPointerID(const std::string& type, uintptr_t ptr) {
        std::string key = type + "_" + std::to_string(ptr);
        auto it = id_registry_.find(key);
        return it != id_registry_.end() ? it->second : 0;
    }

    void unregisterPointer(uintptr_t id) {
        auto it = pointer_registry_.find(id);
        if (it != pointer_registry_.end()) {
            std::string type = it->second;
            pointer_registry_.erase(it);
            id_registry_.erase(type + "_" + std::to_string(id));
        }
    }
};

// Conversores de tipos
class TypeConverters {
public:
    // Converte std::unordered_map<ENetPeer*, Player> para tabela Lua
    static sol::table convertPlayerMapToLua(sol::state& lua, 
                                          const std::unordered_map<ENetPeer*, Player>& players) {
        sol::table result = lua.create_table();
        for (const auto& [peer, player] : players) {
            if (peer) {
                uintptr_t peer_id = PointerManager::getInstance().registerPointer(peer, "ENetPeer");
                sol::table player_table = lua.create_table();
                player_table["id"] = player.id;
                player_table["username"] = player.username;
                player_table["x"] = player.x;
                player_table["y"] = player.y;
                result[peer_id] = player_table;
            }
        }
        return result;
    }

    // Converte nlohmann::json para tabela Lua
    static sol::table convertJsonToLua(sol::state& lua, const json& j) {
        sol::table result = lua.create_table();
        for (auto& [key, value] : j.items()) {
            if (value.is_string()) {
                result[key] = value.get<std::string>();
            } else if (value.is_number_integer()) {
                result[key] = value.get<int>();
            } else if (value.is_number_float()) {
                result[key] = value.get<double>();
            } else if (value.is_boolean()) {
                result[key] = value.get<bool>();
            } else if (value.is_array()) {
                sol::table arr = lua.create_table();
                for (size_t i = 0; i < value.size(); ++i) {
                    arr[i + 1] = convertJsonValueToLua(lua, value[i]);
                }
                result[key] = arr;
            } else if (value.is_object()) {
                result[key] = convertJsonToLua(lua, value);
            } else {
                result[key] = sol::nil;
            }
        }
        return result;
    }

    // Converte valor JSON individual para Lua
    static sol::object convertJsonValueToLua(sol::state& lua, const json& value) {
        if (value.is_string()) {
            return sol::make_object(lua, value.get<std::string>());
        } else if (value.is_number_integer()) {
            return sol::make_object(lua, value.get<int>());
        } else if (value.is_number_float()) {
            return sol::make_object(lua, value.get<double>());
        } else if (value.is_boolean()) {
            return sol::make_object(lua, value.get<bool>());
        } else if (value.is_array()) {
            sol::table arr = lua.create_table();
            for (size_t i = 0; i < value.size(); ++i) {
                arr[i + 1] = convertJsonValueToLua(lua, value[i]);
            }
            return sol::make_object(lua, arr);
        } else if (value.is_object()) {
            return sol::make_object(lua, convertJsonToLua(lua, value));
        } else {
            return sol::make_object(lua, sol::nil);
        }
    }

    // Converte tabela Lua para nlohmann::json
    static json convertLuaToJson(sol::table table) {
        json result;
        for (auto&& [key, value] : table) {
            std::string key_str = key.as<std::string>();
            result[key_str] = convertLuaValueToJson(value);
        }
        return result;
    }

    // Converte valor Lua individual para JSON
    static json convertLuaValueToJson(sol::object value) {
        if (value.is<std::string>()) {
            return value.as<std::string>();
        } else if (value.is<int>()) {
            return value.as<int>();
        } else if (value.is<double>()) {
            return value.as<double>();
        } else if (value.is<bool>()) {
            return value.as<bool>();
        } else if (value.is<sol::table>()) {
            return convertLuaToJson(value.as<sol::table>());
        } else {
            return nullptr;
        }
    }
};

// Classe principal da interface Lua unificada
class LuaUnifiedInterface {
private:
    std::unique_ptr<sol::state> lua_;
    std::unordered_map<std::string, std::function<void()>> module_initializers_;
    Server* server_;
    
    // Instâncias dos gerenciadores (para acesso via Lua)
    ConfigManager* config_manager_;
    SecureDatabase* database_;
    SecurePacketHandler* packet_handler_;
    NetworkManager* network_manager_;
    GameManager* game_manager_;
    PlayerManager* player_manager_;
    DatabaseManager* db_manager_;

public:
    LuaUnifiedInterface();
    ~LuaUnifiedInterface();

    // Inicialização e shutdown
    bool initialize();
    void shutdown();
    
    // Carregamento de scripts
    bool loadScript(const std::string& script_name, const std::string& file_path);
    bool loadScriptFromString(const std::string& script_name, const std::string& code);
    bool loadAllScripts(const std::string& scripts_directory);
    
    // Execução de funções
    template<typename... Args>
    bool callFunction(const std::string& function_name, Args... args);
    
    template<typename... Args>
    bool callFunctionInScript(const std::string& script_name, const std::string& function_name, Args... args);
    
    // Acesso ao estado Lua
    sol::state& getLuaState() { return *lua_; }
    const sol::state& getLuaState() const { return *lua_; }
    
    // Configuração de instâncias
    void setServer(Server* server) { server_ = server; }
    void setConfigManager(ConfigManager* config) { config_manager_ = config; }
    void setDatabase(SecureDatabase* db) { database_ = db; }
    void setPacketHandler(SecurePacketHandler* handler) { packet_handler_ = handler; }
    void setNetworkManager(NetworkManager* network) { network_manager_ = network; }
    void setGameManager(GameManager* game) { game_manager_ = game; }
    void setPlayerManager(PlayerManager* player) { player_manager_ = player; }
    void setDatabaseManager(DatabaseManager* db) { db_manager_ = db; }
    
    // Registro de módulos
    void registerAllModules();
    void registerConfigManager();
    void registerSecureDatabase();
    void registerSecurePacketHandler();
    void registerNetworkManager();
    void registerGameManager();
    void registerPlayerManager();
    void registerDatabaseManager();
    void registerUtils();
    
    // Funções utilitárias
    bool isScriptLoaded(const std::string& script_name) const;
    void listLoadedScripts() const;
    
private:
    // Registro de funções utilitárias
    void registerUtilityFunctions();
    
    // Validação de scripts
    bool isValidScriptPath(const std::string& file_path) const;
    
    // Carregamento individual de script
    bool loadIndividualScript(const std::string& script_name, const std::string& content);
};

// Instância global para compatibilidade
extern std::unique_ptr<LuaUnifiedInterface> luaUnifiedInterface;

// Funções de compatibilidade com código existente
bool initLua();
void shutdownLua();
bool loadLuaScript(const std::string& script_name, const std::string& file_path);
template<typename... Args>
bool callLuaFunction(const std::string& script_name, const std::string& function_name, Args... args);

} // namespace LuaUnified