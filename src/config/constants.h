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

#include <string>

namespace Config {
    // ==============================
    // Configurações do Servidor
    // ==============================
    constexpr int SERVER_PORT = 7777;                    ///< Porta do servidor
    constexpr int MAX_CLIENTS = 32;                      ///< Número máximo de clientes
    constexpr int MAX_CHANNELS = 2;                      ///< Número de canais de rede
    constexpr int SERVER_TIMEOUT_MS = 1000;              ///< Timeout do servidor em ms
    constexpr int INACTIVE_PLAYER_CLEANUP_INTERVAL = 30; ///< Intervalo de limpeza em segundos
    constexpr int INACTIVE_PLAYER_TIMEOUT = 5;           ///< Timeout de inatividade em minutos
    
    // ==============================
    // Configurações de Banco de Dados
    // ==============================
    constexpr const char* PLAYERS_TABLE = "players";    ///< Nome da tabela de jogadores
    
    // ==============================
    // Configurações de Rede
    // ==============================
    constexpr const char* DEFAULT_HOST = "ENET_HOST_ANY"; ///< Host padrão
    
    // ==============================
    // Configurações de Scripts
    // ==============================
    constexpr const char* SCRIPTS_DIRECTORY = "scripts";      ///< Diretório dos scripts Lua
    constexpr const char* CHAT_SCRIPTS = "chat_commands";    ///< Script de comandos de chat
    constexpr const char* GAME_LOGIC_SCRIPT = "game_logic";    ///< Script de lógica do jogo
    
    // ==============================
    // Configurações de Comandos
    // ==============================
    constexpr const char* HELP_COMMAND = "/ajuda";           ///< Comando de ajuda
    constexpr const char* PLAYERS_COMMAND = "/jogadores";    ///< Comando de listar jogadores
    constexpr const char* TIME_COMMAND = "/tempo";           ///< Comando de tempo
    
    // ==============================
    // Templates de Mensagens
    // ==============================
    constexpr const char* LOGIN_SUCCESS_MSG = "Login OK";    ///< Mensagem de login bem-sucedido
    constexpr const char* HELP_MSG = "Comandos disponíveis: /ajuda, /jogadores, /tempo";
    constexpr const char* UNKNOWN_COMMAND_MSG = "Comando não reconhecido. Use /ajuda para ajuda.";
    constexpr const char* LUA_ERROR_MSG = "Falha ao processar comando. Sistema Lua pode estar com problemas.";
    
    // ==============================
    // Prefixos de Log
    // ==============================
    constexpr const char* LOG_PREFIX_PLAYER = "[PLAYER]";   ///< Prefixo para logs de jogador
    constexpr const char* LOG_PREFIX_LOGIN = "[LOGIN]";     ///< Prefixo para logs de login
    constexpr const char* LOG_PREFIX_CHAT = "[CHAT]";       ///< Prefixo para logs de chat
    constexpr const char* LOG_PREFIX_MOVE = "[MOVE]";       ///< Prefixo para logs de movimento
    constexpr const char* LOG_PREFIX_LUA = "[LUA]";         ///< Prefixo para logs de Lua
    constexpr const char* LOG_PREFIX_DEBUG = "[DEBUG]";     ///< Prefixo para logs de debug
    constexpr const char* LOG_PREFIX_ERROR = "[ERRO]";      ///< Prefixo para logs de erro
    
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