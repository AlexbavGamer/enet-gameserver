/**
 * @file constants.h
 * @brief Constantes de configuração do servidor
 *
 * Este arquivo centraliza todas as constantes de configuração do sistema,
 * organizadas por categorias para facilitar manutenção e modificação.
 * As constantes são definidas no namespace Config para evitar conflitos.
 *
 * @author Sistema de Servidor Seguro
 * @version 1.0
 * @date 2025
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#ifdef HELP_COMMAND
#undef HELP_COMMAND
#endif

#include <string>

namespace Config {
    // ==============================
    // Configurações do Servidor
    // ==============================
    constexpr int SERVER_PORT = 7777;                    
    constexpr int MAX_CLIENTS = 32;                      
    constexpr int MAX_CHANNELS = 2;                      
    constexpr int SERVER_TIMEOUT_MS = 1000;              
    constexpr int INACTIVE_PLAYER_CLEANUP_INTERVAL = 30; 
    constexpr int INACTIVE_PLAYER_TIMEOUT = 5;           
    
    // ==============================
    // Configurações de Banco de Dados
    // ==============================
    constexpr const char* PLAYERS_TABLE = "players";    
    
    // ==============================
    // Configurações de Rede
    // ==============================
    constexpr const char* DEFAULT_HOST = "ENET_HOST_ANY"; 
    
    // ==============================
    // Configurações de Scripts
    // ==============================
    constexpr const char* SCRIPTS_DIRECTORY = "scripts";      
    constexpr const char* CHAT_SCRIPTS = "chat_commands";    
    constexpr const char* GAME_LOGIC_SCRIPT = "game_logic";    
    
    // ==============================
    // Configurações de Comandos
    // ==============================
    constexpr const char* HELP_COMMAND = "/ajuda";    
    constexpr const char* PLAYERS_COMMAND = "/jogadores";    
    constexpr const char* TIME_COMMAND = "/tempo";           
    
    // ==============================
    // Templates de Mensagens
    // ==============================
    constexpr const char* LOGIN_SUCCESS_MSG = "Login OK";    
    constexpr const char* HELP_MSG = "Comandos disponíveis: /ajuda, /jogadores, /tempo";
    constexpr const char* UNKNOWN_COMMAND_MSG = "Comando não reconhecido. Use /ajuda para ajuda.";
    constexpr const char* LUA_ERROR_MSG = "Falha ao processar comando. Sistema Lua pode estar com problemas.";
    
    // ==============================
    // Prefixos de Log
    // ==============================
    constexpr const char* LOG_PREFIX_PLAYER = "[PLAYER]";   
    constexpr const char* LOG_PREFIX_LOGIN = "[LOGIN]";     
    constexpr const char* LOG_PREFIX_CHAT = "[CHAT]";       
    constexpr const char* LOG_PREFIX_MOVE = "[MOVE]";       
    constexpr const char* LOG_PREFIX_LUA = "[LUA]";         
    constexpr const char* LOG_PREFIX_DEBUG = "[DEBUG]";     
    constexpr const char* LOG_PREFIX_ERROR = "[ERRO]";      
    
    // ==============================
    // Mensagens de Erro
    // ==============================
    constexpr const char* DB_INIT_ERROR = "Falha ao inicializar banco de dados seguro";
    constexpr const char* LUA_INIT_ERROR = "Falha ao inicializar sistema Lua, continuando sem Lua...";
    constexpr const char* ENET_INIT_ERROR = "Erro ao inicializar ENet.";
    constexpr const char* SERVER_CREATE_ERROR = "Erro ao criar servidor.";
    constexpr const char* FATAL_ERROR_PREFIX = "Erro fatal: ";
}

#endif // CONSTANTS_H