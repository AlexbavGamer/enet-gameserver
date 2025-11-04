/**
 * @file server.cpp
 * @brief Implementação da classe principal do servidor
 *
 * Este arquivo contém a implementação dos métodos da classe Server,
 * responsável por coordenar todos os componentes do sistema multiplayer.
 */

#include "server.h"
#include "../secure_packet_handler.h"
#include "../lua_unified.h"
#include <nlohmann/json.hpp>
#include <console_utils.h>
#include <thread>

using json = nlohmann::json;

/**
 * @brief Construtor da classe Server
 *
 * Inicializa todos os gerenciadores do sistema na ordem correta:
 * 1. NetworkManager - para operações de rede
 * 2. DatabaseManager - para operações de banco de dados
 * 3. PlayerManager - para gerenciamento de jogadores
 * 4. GameManager - para lógica do jogo
 * 5. LuaManager - para integração com scripts Lua
 *
 * O estado inicial do servidor é definido como não rodando (running_ = false)
 * e o timestamp da última limpeza é definido para o momento atual.
 */
Server::Server()
    : network_manager_(std::make_unique<NetworkManager>())
    , database_manager_(std::make_unique<DatabaseManager>())
    , config_manager_(std::make_unique<ConfigManager>())
    , player_manager_(std::make_unique<PlayerManager>(std::make_unique<DatabaseManager>()))
    , lua_manager_(std::make_unique<LuaManager>(this))
    , lua_unified_(std::make_unique<LuaUnified::LuaUnifiedInterface>())
    , running_(false)
    , last_cleanup_(std::chrono::steady_clock::now()) {
    
    // Criar GameManager depois que o construtor da lista de inicialização terminar
    game_manager_ = std::make_unique<GameManager>(std::move(player_manager_));
}

Server::~Server() {
    shutdown();
}

bool Server::initialize() {
    // Configurar encoding do console
    ConsoleUtils::setupConsoleEncoding();
    
    // Inicializar componentes
    if (!initializeComponents()) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao inicializar componentes do servidor");
        return false;
    }
    
    running_ = true;
    printStartupInfo();
    
    return true;
}

void Server::run() {
    if (!running_) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Servidor não inicializado");
        return;
    }
    
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Servidor iniciado. Pressione Ctrl+C para parar.");
    
    int iteration = 0;
    while (running_) {
        iteration++;
        
        processEvents();
        performMaintenance();
        
        // Pequena pausa para não consumir 100% da CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void Server::shutdown() {
    if (!running_) {
        return;
    }
    
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Desligando servidor...");
    
    running_ = false;
    
    // Shutdown dos componentes na ordem correta
    if (lua_unified_) {
        lua_unified_->shutdown();
    }
    
    if (network_manager_) {
        network_manager_->shutdown();
    }
    
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Servidor desligado com sucesso");
}

bool Server::initializeComponents() {
    // Inicializar NetworkManager
    if (!network_manager_->initialize()) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao inicializar NetworkManager");
        return false;
    }
    
    // Criar servidor
    if (!network_manager_->createServer(Config::SERVER_PORT, Config::MAX_CLIENTS, Config::MAX_CHANNELS)) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao criar servidor");
        return false;
    }
    
    // Inicializar DatabaseManager
    if (!database_manager_->initialize()) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao inicializar DatabaseManager");
        return false;
    }
    
    // Inicializar GameManager
    if (!game_manager_->initialize()) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao inicializar GameManager");
        return false;
    }
    
    // Inicializar Lua Unificado
    if (!lua_unified_->initialize()) {
        LOG_WARNING(Config::LOG_PREFIX_LUA, Config::LUA_INIT_ERROR);
    } else {
        // Configurar os módulos na interface unificada
        lua_unified_->setServer(this);
        lua_unified_->setConfigManager(config_manager_.get());
        lua_unified_->setDatabase(nullptr); // SecureDatabase será implementado como módulo independente
        lua_unified_->setPacketHandler(nullptr); // SecurePacketHandler será implementado como módulo independente
        lua_unified_->setNetworkManager(network_manager_.get());
        lua_unified_->setGameManager(game_manager_.get());
        lua_unified_->setPlayerManager(game_manager_->getPlayerManager());
        lua_unified_->setDatabaseManager(database_manager_.get());
        
        // Registrar os módulos globais para scripts de teste
        lua_unified_->registerConfigManager();
        lua_unified_->registerGameManager();
        lua_unified_->registerPlayerManager();
        lua_unified_->registerDatabaseManager();
        lua_unified_->registerNetworkManager();
        lua_unified_->registerSecurePacketHandler();

        // Carregar scripts Lua
        if (!loadLuaScripts()) {
            LOG_WARNING(Config::LOG_PREFIX_LUA, "Falha ao carregar alguns scripts Lua");
        }
    }
    
    // Setup handlers de pacotes
    if (!setupPacketHandlers()) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Falha ao configurar handlers de pacotes");
        return false;
    }
    
    return true;
}

void Server::processEvents() {
    ENetEvent event;
    int result = network_manager_->service(&event, Config::SERVER_TIMEOUT_MS);
    
    if (result > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                handleConnect(event);
                break;
                
            case ENET_EVENT_TYPE_RECEIVE:
                handleReceive(event);
                break;
                
            case ENET_EVENT_TYPE_DISCONNECT:
                handleDisconnect(event);
                break;
        }
    }
}

void Server::handleConnect(ENetEvent& event) {
    game_manager_->handlePlayerConnect(event.peer);
}

void Server::handleReceive(ENetEvent& event) {
    // Processar pacote recebido
    static SecurePacketHandler packet_handler;
    packet_handler.processPacket(network_manager_->getServer(), event);
}

void Server::handleDisconnect(ENetEvent& event) {
    game_manager_->handlePlayerDisconnect(event.peer);
}

void Server::performMaintenance() {
    auto now = std::chrono::steady_clock::now();
    
    // Limpar jogadores inativos a cada intervalo definido
    if (now - last_cleanup_ > std::chrono::seconds(Config::INACTIVE_PLAYER_CLEANUP_INTERVAL)) {
        cleanupInactivePlayers();
        last_cleanup_ = now;
    }
}

void Server::cleanupInactivePlayers() {
    if (game_manager_) {
        game_manager_->getPlayerManager()->cleanupInactivePlayers(std::chrono::minutes(Config::INACTIVE_PLAYER_TIMEOUT));
    }
}

bool Server::loadLuaScripts() {
    return lua_unified_->loadAllScripts(Config::SCRIPTS_DIRECTORY);
}

bool Server::setupPacketHandlers() {
    // Os handlers são configurados dentro do GameManager
    return true;
}

void Server::printStartupInfo() {
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "==========================================");
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "      Servidor Seguro Multiplayer v1.0    ");
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "==========================================");
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Porta: " + std::to_string(Config::SERVER_PORT));
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Clientes máximos: " + std::to_string(Config::MAX_CLIENTS));
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Canais: " + std::to_string(Config::MAX_CHANNELS));
    
    if (lua_manager_) {
        LOG_INFO(Config::LOG_PREFIX_DEBUG, "Suporte a Lua: Ativado");
        lua_manager_->listLoadedScripts();
    } else {
        LOG_WARNING(Config::LOG_PREFIX_LUA, "Suporte a Lua: Desativado");
    }
    
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "==========================================");
}