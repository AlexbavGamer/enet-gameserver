#pragma once
#include <cstdint>

// ==============================
// 1. ENUM DE PACOTES (UNIFICADO)
// ==============================
enum PacketID : uint8_t {
    PING = 1,
    LOGIN,
    LOGOUT,
    MOVE,
    CHAT,
    SPAWN_PLAYER,
    LUA_SCRIPT,
    LUA_RESPONSE
};

// ==============================
// 2. CONFIGURAÇÃO DO SERVIDOR
// ==============================
struct ServerConfig {
    int port = 7777;
    int max_clients = 32;
    int channels = 2;
    int timeout_ms = 1000;
    int cleanup_interval_seconds = 30;
    int player_inactivity_timeout_minutes = 5;
    
    // Database configuration
    std::string db_connection = "db=game_db user=root host=127.0.0.1 port=3306";
    std::string db_table = "players";
    
    // Lua configuration
    std::string scripts_path = "scripts";
    
    // Performance settings
    bool enable_binary_protocol = false;
    int binary_protocol_threshold = 10; // packets per second
};

// ==============================
// 3. LOG LEVELS
// ==============================
enum LogLevel : int {
    LOG_ERROR = 0,
    LOG_WARNING = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3
};

// ==============================
// 4. PACKET FLAGS
// ==============================
const uint32_t PACKET_FLAG_RELIABLE = ENET_PACKET_FLAG_RELIABLE;
const uint32_t PACKET_FLAG_UNRELIABLE = ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT;
const uint32_t PACKET_FLAG_UNSEQUENCED = ENET_PACKET_FLAG_UNSEQUENCED;