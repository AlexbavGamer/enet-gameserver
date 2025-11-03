/**
 * @file server.h
 * @brief Classe principal do servidor multiplayer seguro
 *
 * Esta classe coordena todos os componentes do sistema:
 * - Gerenciamento de rede (NetworkManager)
 * - Operações de banco de dados (DatabaseManager)
 * - Gerenciamento de jogadores (PlayerManager)
 * - Lógica do jogo (GameManager)
 * - Integração com Lua (LuaManager)
 *
 * @author Sistema de Servidor Seguro
 * @version 1.0
 * @date 2025
 */

#ifndef SERVER_H
#define SERVER_H

#include "../config/constants.h"
#include "../utils/logger.h"
#include "../database/database_manager.h"
#include "../server/player_manager.h"
#include "../network/network_manager.h"
#include "../game/game_manager.h"
#include "../game/lua_manager.h"
#include <enet/enet.h>
#include <memory>
#include <chrono>

/**
 * @class Server
 * @brief Classe principal que gerencia o ciclo de vida do servidor
 *
 * A classe Server atua como um facade que coordena todos os componentes
 * do sistema multiplayer. Ela é responsável por:
 * - Inicializar todos os componentes em ordem correta
 * - Gerenciar o loop principal do servidor
 * - Lidar com eventos de rede (conexão, recebimento, desconexão)
 * - Manter a limpeza de jogadores inativos
 * - Carregar scripts Lua
 * - Realizar manutenção periódica do sistema
 */
class Server {
private:
    // Componentes do sistema
    std::unique_ptr<NetworkManager> network_manager_;  ///< Gerenciador de operações de rede
    std::unique_ptr<DatabaseManager> database_manager_; ///< Gerenciador de banco de dados
    std::unique_ptr<PlayerManager> player_manager_;    ///< Gerenciador de jogadores
    std::unique_ptr<GameManager> game_manager_;        ///< Gerenciador de lógica do jogo
    std::unique_ptr<LuaManager> lua_manager_;          ///< Gerenciador de scripts Lua
    
    // Estado do servidor
    bool running_;                                     ///< Flag indicando se o servidor está rodando
    std::chrono::steady_clock::time_point last_cleanup_; ///< Timestamp da última limpeza
    
public:
    /**
     * @brief Construtor padrão
     *
     * Inicializa todos os gerenciadores como nullptr. A inicialização
     * completa é feita pelo método initialize().
     */
    Server();
    
    /**
     * @brief Destrutor
     *
     * Garante que todos os componentes sejam desligados corretamente
     * na ordem inversa da inicialização.
     */
    ~Server();
    
    // Ciclo de vida do servidor
    bool initialize();  ///< Inicializa todos os componentes do servidor
    void run();         ///< Inicia o loop principal do servidor
    void shutdown();    ///< Desliga todos os componentes de forma segura
    
    // Verificação de status
    bool isRunning() const { return running_; }  ///< Verifica se o servidor está rodando
    
    // Acesso aos componentes
    GameManager* getGameManager() const { return game_manager_.get(); }
    LuaManager* getLuaManager() const { return lua_manager_.get(); }
    
private:
    // Inicialização dos componentes
    bool initializeComponents();  ///< Inicializa todos os componentes em ordem correta
    
    // Loop principal do servidor
    void processEvents();        ///< Processa eventos de rede
    void handleConnect(ENetEvent& event);    ///< Lida com conexões de clientes
    void handleReceive(ENetEvent& event);    ///< Lida com pacotes recebidos
    void handleDisconnect(ENetEvent& event); ///< Lida com desconexões de clientes
    
    // Manutenção
    void performMaintenance();   ///< Realiza manutenção periódica do sistema
    void cleanupInactivePlayers(); ///< Remove jogadores inativos
    
    // Carregamento de scripts
    bool loadLuaScripts();       ///< Carrega todos os scripts Lua da pasta scripts/
    
    // Métodos auxiliares
    bool setupPacketHandlers();  ///< Configura handlers de pacotes
    void printStartupInfo();     ///< Exibe informações de inicialização
};

#endif // SERVER_H