#include <enet/enet.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include "encoding_utils.h"
#include "server_common.h"

// Inclusões dos novos componentes seguros
#include "secure_database.h"
#include "secure_packet_handler.h"
#include "lua_interface.h"

using json = nlohmann::json;

// ==============================
// 3. Estrutura do Jogador (melhorada)
// ==============================
struct Player {
    int id = 0;
    std::string username = "unknown";
    float x = 0, y = 0;
    std::chrono::steady_clock::time_point last_activity;
    
    Player() : last_activity(std::chrono::steady_clock::now()) {}
};

// ==============================
// 4. Gerenciamento de Jogadores Seguro
// ==============================
class PlayerManager {
private:
    std::unordered_map<ENetPeer*, Player> players_;
    int next_player_id_ = 1;
    std::unique_ptr<SecureDatabase> db_;
    
public:
    PlayerManager(std::unique_ptr<SecureDatabase> db) : db_(std::move(db)) {}
    
    // Adiciona novo jogador
    int addPlayer(ENetPeer* peer, const std::string& username) {
        Player& player = players_[peer];
        player.id = next_player_id_++;
        player.username = username;
        player.last_activity = std::chrono::steady_clock::now();
        
        // Salvar no banco de dados
        json player_data = {
            {"username", username},
            {"x", 0.0},
            {"y", 0.0},
            {"created_at", "CURRENT_TIMESTAMP"}
        };
        
        if (db_ && db_->create("players", player_data)) {
            std::cout << "[PLAYER] Jogador " << username << " (ID: " << player.id << ") salvo no banco de dados" << std::endl;
        }
        
        return player.id;
    }
    
    // Remove jogador
    void removePlayer(ENetPeer* peer) {
        auto it = players_.find(peer);
        if (it != players_.end()) {
            int id = it->second.id;
            std::cout << "[PLAYER] Jogador " << id << " desconectado" << std::endl;
            
            // Remover do banco de dados
            if (db_) {
                db_->remove("players", "id = " + std::to_string(id));
            }
            
            players_.erase(it);
        }
    }
    
    // Atualiza posição do jogador
    void updatePosition(ENetPeer* peer, float x, float y) {
        auto it = players_.find(peer);
        if (it != players_.end()) {
            it->second.x = x;
            it->second.y = y;
            it->second.last_activity = std::chrono::steady_clock::now();
            
            // Atualizar no banco de dados
            if (db_) {
                json update_data = {{"x", x}, {"y", y}};
                db_->update("players", update_data, "id = " + std::to_string(it->second.id));
            }
        }
    }
    
    // Obtém jogador por peer
    Player* getPlayer(ENetPeer* peer) {
        auto it = players_.find(peer);
        return it != players_.end() ? &it->second : nullptr;
    }
    
    // Obtém todos os jogadores
    const std::unordered_map<ENetPeer*, Player>& getAllPlayers() const {
        return players_;
    }
    
    // Limpa jogadores inativos
    void cleanupInactivePlayers(std::chrono::steady_clock::duration timeout) {
        auto now = std::chrono::steady_clock::now();
        auto it = players_.begin();
        
        while (it != players_.end()) {
            if (now - it->second.last_activity > timeout) {
                std::cout << "[PLAYER] Removendo jogador inativo: " << it->second.username << std::endl;
                it = players_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

// ==============================
// 5. Instâncias Globais
// ==============================
std::unique_ptr<PlayerManager> playerManager;
std::unique_ptr<SecurePacketHandler> packetHandler;

// ==============================
// 6. Handlers de Pacotes Específicos
// ==============================
void handleLogin(ENetHost* server, ENetEvent& event, const json& data) {
    if (!data.contains("user") || !data["user"].is_string()) {
        std::cerr << "[LOGIN ERRO] Login sem 'user' válido" << std::endl;
        return;
    }
    
    std::string user = data["user"].get<std::string>();
    std::cout << "[LOGIN] Login solicitado: " << user << std::endl;

    // Adicionar jogador
    int playerId = playerManager->addPlayer(event.peer, user);
    
    // Enviar resposta de login
    auto login_data = json{{"msg", "Login OK"}, {"id", playerId}};
    auto login_reply = SecurePacketHandler::create(LOGIN, login_data);
    if (login_reply) {
        SecurePacketHandler::sendPacket(event.peer, std::move(login_reply));
    }

    // Spawn para outros jogadores
    auto spawn_packet = createSpawnPlayerPacket(playerId, user, 0, 0);
    if (spawn_packet) {
        packetHandler->broadcastExcept(server, event.peer, std::move(spawn_packet));
    }

    // Enviar jogadores existentes para o novo jogador
    for (const auto& [peer, player] : playerManager->getAllPlayers()) {
        if (peer != event.peer && player.id > 0) {
            auto other_packet = createSpawnPlayerPacket(player.id, player.username, player.x, player.y);
            if (other_packet) {
                SecurePacketHandler::sendPacket(event.peer, std::move(other_packet));
            }
        }
    }
}

void handleMove(ENetHost* server, ENetEvent& event, const json& data) {
    // Validação forte
    if (!data.contains("x") || !data["x"].is_number() ||
        !data.contains("y") || !data["y"].is_number()) {
        std::cerr << "[MOVE ERRO] Pacote MOVE inválido!" << std::endl;
        return;
    }

    float x = data["x"].get<float>();
    float y = data["y"].get<float>();

    Player* player = playerManager->getPlayer(event.peer);
    if (!player) return;

    // Atualizar posição
    playerManager->updatePosition(event.peer, x, y);

    // SERVIDOR adiciona o ID (cliente NÃO envia!)
    json move_data = { {"id", player->id}, {"x", x}, {"y", y} };
    auto move_packet = SecurePacketHandler::create(MOVE, move_data);
    if (move_packet) {
        packetHandler->broadcastExcept(server, nullptr, std::move(move_packet));
        std::cout << "[DEBUG] MOVE recebido: x=" << x << " y=" << y << " de jogador ID=" << player->id << std::endl;
    }
}

void handleChat(ENetHost* server, ENetEvent& event, const json& data) {
    if (!data.contains("msg") || !data["msg"].is_string()) {
        safePrint("[CHAT ERRO] Pacote CHAT inválido: campo 'msg' ausente ou não é string");
        safePrint("[CHAT DEBUG] Dados recebidos: " + data.dump());
        return;
    }
    
    std::string text = data["msg"].get<std::string>();
    Player* player = playerManager->getPlayer(event.peer);
    
    if (player) {
        safePrint("[CHAT] " + player->username + ": " + text);
        
        // Verificar se é um comando
        if (text.starts_with("/")) {
            safePrint("[COMANDO] Comando recebido: " + text + " (processando via Lua)");
            
            // Verificar se o sistema Lua está disponível
            if (!luaManager) {
                safePrint("[LUA ERRO] Sistema Lua não inicializado, processando comando em C++ fallback");
                
                // Processar comando básico em C++
                if (text == "/ajuda") {
                    std::string help_msg = "Comandos disponíveis: /ajuda, /jogadores, /tempo";
                    json response_data = { {"user", "Sistema"}, {"msg", help_msg} };
                    auto response_packet = SecurePacketHandler::create(CHAT, response_data);
                    if (response_packet) {
                        SecurePacketHandler::sendPacket(event.peer, std::move(response_packet));
                    }
                } else {
                    json response_data = { {"user", "Sistema"}, {"msg", "Comando não reconhecido. Use /ajuda para ajuda."} };
                    auto response_packet = SecurePacketHandler::create(CHAT, response_data);
                    if (response_packet) {
                        SecurePacketHandler::sendPacket(event.peer, std::move(response_packet));
                    }
                }
                return;
            }
            
            // Verificar se o script de chat commands está carregado
            safePrint("[LUA DEBUG] Verificando se script 'chat_commands' está carregado...");
            if (luaManager->lua["chat_commands"].valid()) {
                safePrint("[LUA DEBUG] Script 'chat_commands' encontrado, verificando função 'processAdvancedCommand'...");
                
                // Executar script de comandos
                if (callLuaFunction("chat_commands", "processAdvancedCommand",
                    player->id, player->username, text)) {
                    safePrint("[LUA DEBUG] Comando processado com sucesso pelo Lua");
                    return;
                } else {
                    safePrint("[LUA ERRO] Falha ao processar comando via Lua, tentando fallback C++");
                }
            } else {
                safePrint("[LUA ERRO] Script 'chat_commands' não encontrado ou inválido");
                safePrint("[LUA DEBUG] Scripts carregados: ");
                for (const auto& script_name : luaManager->scripts) {
                    safePrint("[LUA DEBUG]  - " + script_name.first);
                }
            }
            
            // Fallback se Lua não processou o comando
            json response_data = { {"user", "Sistema"}, {"msg", "Falha ao processar comando. Sistema Lua pode estar com problemas."} };
            auto response_packet = SecurePacketHandler::create(CHAT, response_data);
            if (response_packet) {
                SecurePacketHandler::sendPacket(event.peer, std::move(response_packet));
            }
        } else {
            // Chat normal - pode ser processado por Lua também
            safePrint("[CHAT DEBUG] Processando mensagem de chat normal via Lua...");
            
            if (luaManager) {
                if (luaManager->lua["game_logic"].valid()) {
                    safePrint("[LUA DEBUG] Script 'game_logic' encontrado, verificando função 'onChatMessage'...");
                    
                    if (callLuaFunction("game_logic", "onChatMessage",
                        player->id, player->username, text)) {
                        safePrint("[LUA DEBUG] Mensagem de chat processada com sucesso pelo Lua");
                    } else {
                        safePrint("[LUA ERRO] Falha ao processar mensagem de chat via Lua, continuando com processamento normal");
                    }
                } else {
                    safePrint("[LUA ERRO] Script 'game_logic' não encontrado ou inválido");
                }
            } else {
                safePrint("[LUA ERRO] Sistema Lua não disponível, processando chat em modo normal");
            }
            
            // Chat normal
            json chat_data = { {"user", player->username}, {"msg", text} };
            auto chat_packet = SecurePacketHandler::create(CHAT, chat_data);
            if (chat_packet) {
                packetHandler->broadcastExcept(server, nullptr, std::move(chat_packet));
            }
        }
    } else {
        safePrint("[CHAT ERRO] Jogador não encontrado para o pacote de chat");
        safePrint("[CHAT DEBUG] Peer: " + std::to_string(reinterpret_cast<uintptr_t>(event.peer)));
    }
}

// ==============================
// 7. Setup dos Handlers
// ==============================
void setupPacketHandlers() {
    packetHandler = std::make_unique<SecurePacketHandler>();
    
    // Registrar handlers específicos
    packetHandler->registerHandler(LOGIN, handleLogin);
    packetHandler->registerHandler(MOVE, handleMove);
    packetHandler->registerHandler(CHAT, handleChat);
}

// ==============================
// 8. Main (Versão Segura com Lua)
// ==============================
int main() {
    try {
        // Configurar encoding do console antes de qualquer saída
        setupConsoleEncoding();
        
        // Inicializar banco de dados seguro
        auto db = std::make_unique<SecureDatabase>();
        if (!db || !db->isValidTableName("players")) {
            std::cerr << "Falha ao inicializar banco de dados seguro" << std::endl;
            return 1;
        }
        
        // Inicializar sistema Lua
        if (!initLua()) {
            std::cerr << "Falha ao inicializar sistema Lua, continuando sem Lua..." << std::endl;
        } else {
            // Carregar todos os scripts Lua da pasta scripts
            #include <filesystem>
            namespace fs = std::filesystem;
            
            try {
                std::string scripts_path = "scripts";
                if (fs::exists(scripts_path) && fs::is_directory(scripts_path)) {
                    safePrint("[LUA DEBUG] Pasta scripts encontrada: " + scripts_path);
                    for (const auto& entry : fs::directory_iterator(scripts_path)) {
                        if (entry.path().extension() == ".lua") {
                            std::string filename = entry.path().filename().string();
                            std::string script_name = entry.path().stem().string();
                            std::string full_path = entry.path().string();
                            
                            safePrint("[LUA DEBUG] Carregando script: " + filename + " como " + script_name);
                            safePrint("[LUA DEBUG] Caminho completo: " + full_path);
                            
                            if (loadLuaScript(script_name, full_path)) {
                                safePrint("[LUA] Script " + filename + " carregado com sucesso");
                            } else {
                                safePrint("[LUA ERRO] Falha ao carregar script " + filename);
                            }
                        }
                    }
                } else {
                    safePrint("[LUA ERRO] Pasta scripts não encontrada: " + scripts_path);
                    // Tentar caminho alternativo
                    std::string alt_scripts_path = "scripts";
                    if (fs::exists(alt_scripts_path) && fs::is_directory(alt_scripts_path)) {
                        safePrint("[LUA DEBUG] Tentando caminho alternativo: " + alt_scripts_path);
                        for (const auto& entry : fs::directory_iterator(alt_scripts_path)) {
                            if (entry.path().extension() == ".lua") {
                                std::string filename = entry.path().filename().string();
                                std::string script_name = entry.path().stem().string();
                                std::string full_path = entry.path().string();
                                
                                safePrint("[LUA DEBUG] Carregando script do caminho alternativo: " + filename);
                                
                                if (loadLuaScript(script_name, full_path)) {
                                    safePrint("[LUA] Script " + filename + " carregado com sucesso do caminho alternativo");
                                } else {
                                    safePrint("[LUA ERRO] Falha ao carregar script " + filename + " do caminho alternativo");
                                }
                            }
                        }
                    } else {
                        safePrint("[LUA ERRO] Nenhuma pasta de scripts encontrada");
                    }
                }
            } catch (const fs::filesystem_error& e) {
                safePrint("[LUA ERRO] Erro ao acessar pasta scripts: " + std::string(e.what()));
            }
        }
        
        // Inicializar gerenciador de jogadores
        playerManager = std::make_unique<PlayerManager>(std::move(db));
        
        // Inicializar ENet
        if (enet_initialize() != 0) {
            std::cerr << "Erro ao inicializar ENet." << std::endl;
            return 1;
        }
        atexit(enet_deinitialize);

        ENetAddress address{};
        address.host = ENET_HOST_ANY;
        address.port = 7777;

        ENetHost* server = enet_host_create(&address, 32, 2, 0, 0);
        if (!server) {
            std::cerr << "Erro ao criar servidor." << std::endl;
            return 1;
        }

        safePrint("Servidor seguro rodando na porta 7777 (com suporte a Lua)...");

        // Setup handlers de pacotes
        setupPacketHandlers();

        ENetEvent event;
        auto last_cleanup = std::chrono::steady_clock::now();
        
        while (true) {
            int result = enet_host_service(server, &event, 1000);
            if (result <= 0) {
                // Limpar jogadores inativos a cada 30 segundos
                auto now = std::chrono::steady_clock::now();
                if (now - last_cleanup > std::chrono::seconds(30)) {
                    playerManager->cleanupInactivePlayers(std::chrono::minutes(5));
                    last_cleanup = now;
                }
                continue;
            }

            try {
                switch (event.type) {
                    case ENET_EVENT_TYPE_CONNECT: {
                        safePrint("Cliente conectido: " + std::to_string(reinterpret_cast<uintptr_t>(event.peer)));
                        playerManager->getPlayer(event.peer); // Inicializar estrutura do jogador
                        auto ping = createPingPacket();
                        if (ping) {
                            SecurePacketHandler::sendPacket(event.peer, std::move(ping));
                        }
                        
                        // Notificar Lua sobre novo jogador
                        Player* player = playerManager->getPlayer(event.peer);
                        if (player && luaManager) {
                            safePrint("[LUA DEBUG] Notificando Lua sobre novo jogador: " + player->username + " (ID: " + std::to_string(player->id) + ")");
                            if (luaManager->lua["game_logic"].valid()) {
                                if (luaManager->lua["game_logic"]["onPlayerConnect"].valid()) {
                                    callLuaFunction("game_logic", "onPlayerConnect",
                                        player->id, player->username);
                                } else {
                                    safePrint("[LUA ERRO] Função 'onPlayerConnect' não encontrada no script 'game_logic'");
                                }
                            } else {
                                safePrint("[LUA ERRO] Script 'game_logic' não encontrado ou inválido");
                            }
                        }
                        break;
                    }

                    case ENET_EVENT_TYPE_RECEIVE: {
                        packetHandler->processPacket(server, event);
                        break;
                    }

                    case ENET_EVENT_TYPE_DISCONNECT: {
                        safePrint("Cliente desconectado: " + std::to_string(reinterpret_cast<uintptr_t>(event.peer)));
                        Player* player = playerManager->getPlayer(event.peer);
                        if (player) {
                            safePrint("[LUA DEBUG] Notificando Lua sobre desconexão do jogador: " + player->username + " (ID: " + std::to_string(player->id) + ")");
                            
                            // Notificar Lua sobre desconexão
                            if (luaManager) {
                                if (luaManager->lua["game_logic"].valid()) {
                                    if (luaManager->lua["game_logic"]["onPlayerDisconnect"].valid()) {
                                        callLuaFunction("game_logic", "onPlayerDisconnect",
                                            player->id, player->username);
                                    } else {
                                        safePrint("[LUA ERRO] Função 'onPlayerDisconnect' não encontrada no script 'game_logic'");
                                    }
                                } else {
                                    safePrint("[LUA ERRO] Script 'game_logic' não encontrado ou inválido");
                                }
                            }
                            
                            // Notificar outros jogadores
                            auto logout_packet = createLogoutPacket(player->id);
                            if (logout_packet) {
                                packetHandler->broadcastExcept(server, nullptr, std::move(logout_packet));
                            }
                        }
                        playerManager->removePlayer(event.peer);
                        break;
                    }
                }
            }
            catch (const std::exception& e) {
                std::cerr << "Erro no loop principal: " << e.what() << std::endl;
            }
            catch (...) {
                std::cerr << "Erro desconhecido no loop principal!" << std::endl;
            }
        }

        enet_host_destroy(server);
        
        // Finalizar sistema Lua
        shutdownLua();
    }
    catch (const std::exception& e) {
        std::cerr << "Erro fatal: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Erro fatal desconhecido!" << std::endl;
        return 1;
    }

    return 0;
}
