// src/scripting/LuaBindings.cpp
#include "LuaManager.h"
#include "server/Server.h"
#include "server/Player.h"
#include "server/World.h"
#include "database/DatabaseManager.h"
#include <nlohmann/json.hpp>
#include "server/NetworkManager.h"

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
        PacketType pkt_type = PacketType::RPC_CALL; // Parse type string
        std::vector<uint8_t> vec_data(data.begin(), data.end());
        return server->getNetworkManager()->sendPacket(peer_id, pkt_type, vec_data);
    };
    
    lua_["broadcast_packet"] = [server](const std::string& type, const std::string& data) {
        PacketType pkt_type = PacketType::BROADCAST;
        std::vector<uint8_t> vec_data(data.begin(), data.end());
        return server->getNetworkManager()->broadcastPacket(pkt_type, vec_data);
    };
    
    // Server reference
    lua_["server"] = server;
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