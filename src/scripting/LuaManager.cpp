// src/scripting/LuaBindings.cpp
#include "LuaManager.h"
#include "server/Server.h"
#include "server/Player.h"
#include "server/World.h"
#include "utils/CryptoUtils.h"
#include "database/DatabaseManager.h"
#include <nlohmann/json.hpp>
#include "server/NetworkManager.h"
#include <magic_enum/magic_enum.hpp>

// Implementação do construtor
LuaManager::LuaManager() {
    // Inicialização básica do estado Lua
    lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math,
                       sol::lib::string, sol::lib::table);
}

// Implementação do destrutor
LuaManager::~LuaManager() {
    // Limpeza do estado Lua
    lua_.collect_garbage();
}

void LuaManager::registerBindings(Server* server) {
    // Logger bindings
    lua_["log_info"] = [](const std::string& msg) { Logger::info(msg); };
    lua_["log_warning"] = [](const std::string& msg) { Logger::warning(msg); };
    lua_["log_error"] = [](const std::string& msg) { Logger::error(msg); };
    
    // Vector3 binding
    lua_.new_usertype<Vector3>("Vector3",
        sol::constructors<Vector3(), Vector3(float, float, float)>(),
        "x", &Vector3::x,
        "y", &Vector3::y,
        "z", &Vector3::z
    );
    
    lua_.new_usertype<Packet>("Packet",
        "peer_id", &Packet::peer_id,
        "type", &Packet::type,
        "data", [](Packet& pkt) {
            return std::string(reinterpret_cast<const char*>(pkt.data.data()), pkt.data.size());
        }
    );

    lua_.new_usertype<Server>("Server",
        "getDatabaseManager", &Server::getDatabaseManager,
        "getLuaManager", &Server::getLuaManager,
        "getNetworkManager", &Server::getNetworkManager,
        "getWorld", &Server::getWorld
    );

    lua_.new_usertype<NetworkManager>("NetworkManager",
        "getRPCHandler", &NetworkManager::getRPCHandler
    );

    lua_.new_usertype<RPCHandler>("RPCHandler",
        "registerRPCCallback", &RPCHandler::registerRPCCallback,
        "registerRPCCallbackWithId", &RPCHandler::registerRPCCallbackWithId
    );

    // Player binding
    lua_.new_usertype<Player>("Player",
        "get_peer_id", &Player::getPeerId,
        "get_username", &Player::getUsername,
        "get_position", &Player::getPosition,
        "set_position", &Player::setPosition,
        "get_health", &Player::getHealth,
        "set_health", &Player::setHealth
    );
    
    // Database operations (async)
    lua_["db_query"] = [server, this](const std::string& query) -> sol::table {
        auto db = server->getDatabaseManager();
        // Simplified example - real implementation should be async
        auto results = db->executeQuery(query);
        
        sol::table lua_results = lua_.create_table();
        for (size_t i = 0; i < results.size(); ++i) {
            const auto& row = results[i];
            sol::table row_table = lua_.create_table();
            
            // Adiciona cada campo do mapa à tabela Lua
            for (const auto& [key, value] : row) {
                row_table[key] = value;
            }
            
            lua_results[i + 1] = row_table;
        }
        return lua_results;
    };
    
    // Network operations
    lua_["send_packet"] = [server](uint32_t peer_id, const std::string& type, const std::string& data) {
        auto pkt_type = magic_enum::enum_cast<PacketType>(type); // Parse type string
        std::vector<uint8_t> vec_data(data.begin(), data.end());
        if (pkt_type.has_value()) {
            return server->getNetworkManager()->sendPacket(peer_id, pkt_type.value(), vec_data);
        }
    };
    
    lua_["broadcast_packet"] = [server](const std::string& type, const std::string& data) {
        PacketType pkt_type = PacketType::BROADCAST;
        std::vector<uint8_t> vec_data(data.begin(), data.end());
        return server->getNetworkManager()->broadcastPacket(pkt_type, vec_data);
    };

    sol::table crypto_table = lua_.create_table();

    crypto_table["generateSalt"] = CryptoUtils::generateSalt;
    crypto_table["verifyPassword"] = CryptoUtils::verifyPassword;
    crypto_table["generateSessionToken"] = CryptoUtils::generateSessionToken;
    crypto_table["sha256"] = CryptoUtils::sha256;
    crypto_table["hashPassword"] = CryptoUtils::hashPassword;
    
    
    lua_["crypto"] = crypto_table;
    // Server reference
    lua_["server"] = server;

    
    lua_["json_encode"] = [](const sol::object& obj) -> std::string {
        using json = nlohmann::json;

        std::function<nlohmann::json(const sol::object&)> to_json;
        to_json = [&](const sol::object& o) -> nlohmann::json {
            using json = nlohmann::json;

            if (o.is<sol::table>()) {
                json j;
                sol::table t = o.as<sol::table>();
                for (auto& [k, v] : t) {
                    if (k.is<std::string>()) {
                        j[k.as<std::string>()] = to_json(v);
                    } else if (k.is<int>()) {
                        j[std::to_string(k.as<int>())] = to_json(v);
                    }
                }
                return j;
            } else if (o.is<std::string>()) {
                return o.as<std::string>();
            } else if (o.is<double>()) {
                return o.as<double>();
            } else if (o.is<int>()) {
                return o.as<int>();
            } else if (o.is<bool>()) {
                return o.as<bool>();
            } else if (o.is<sol::nil_t>()) {
                return nullptr;
            }
            return nullptr;
        };

        return to_json(obj).dump();
    };

    lua_["json_decode"] = [](const std::string& str, sol::this_state s) -> sol::object {
        using json = nlohmann::json;
        auto j = json::parse(str, nullptr, false);
        sol::state_view lua(s);

        std::function<sol::object(const nlohmann::json&)> to_lua;
        to_lua = [&](const nlohmann::json& val) -> sol::object {
            if (val.is_object()) {
                sol::table t = lua.create_table();
                for (auto& [k, v] : val.items()) {
                    t[k] = to_lua(v);
                }
                return sol::make_object(lua, t);
            } else if (val.is_array()) {
                sol::table t = lua.create_table();
                int i = 1;
                for (auto& v : val) {
                    t[i++] = to_lua(v);
                }
                return sol::make_object(lua, t);
            } else if (val.is_string()) {
                return sol::make_object(lua, val.get<std::string>());
            } else if (val.is_boolean()) {
                return sol::make_object(lua, val.get<bool>());
            } else if (val.is_number_integer()) {
                return sol::make_object(lua, val.get<int>());
            } else if (val.is_number_float()) {
                return sol::make_object(lua, val.get<double>());
            } else {
                return sol::make_object(lua, sol::nil);
            }
        };

        return to_lua(j);
    };
}

bool LuaManager::initialize(Server* server) {
    lua_.open_libraries(sol::lib::base, sol::lib::package, sol::lib::math, 
                       sol::lib::string, sol::lib::table);
    
    registerBindings(server);
    
    // Load init script
    return loadScript("scripts/init.lua");
}

bool LuaManager::loadScript(const std::string& filepath) {
    try {
        lua_.script_file(filepath);
        Logger::info("Loaded Lua script: " + filepath);
        return true;
    } catch (const sol::error& e) {
        Logger::error("Lua script error in " + filepath + ": " + e.what());
        return false;
    }
}