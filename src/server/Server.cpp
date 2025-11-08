// src/server/Server.cpp (atualizado)
#include "server/Server.h"
#include "server/NetworkManager.h"
#include "server/AntiCheat.h"
#include "database/DatabaseManager.h"
#include "scripting/LuaManager.h"
#include "server/World.h"
#include "server/Player.h"
#include "utils/Logger.h"
#include "utils/Config.h"
#include "utils/PerformanceMonitor.h"
#include <chrono>
#include "Server.h"

Server::Server(uint16_t port, size_t max_clients)
    : port_(port), max_clients_(max_clients), running_(false) {}

Server::~Server()
{
    shutdown();
}

bool Server::initialize()
{
    // Carrega configuração
    if (!Config::getInstance().load("config/server_config.json"))
    {
        Logger::warning("Failed to load config, using defaults");
    }

    Logger::info("Initializing server on port " + std::to_string(port_));

    // Inicializa ENet library
    if (enet_initialize() != 0)
    {
        Logger::error("Failed to initialize ENet");
        return false;
    }

    // Cria módulos
    network_manager_ = std::make_unique<NetworkManager>(port_, max_clients_);
    if (!network_manager_->initialize())
    {
        Logger::error("Failed to initialize NetworkManager");
        return false;
    }

    database_manager_ = std::make_unique<DatabaseManager>();
    std::string db_conn = Config::getInstance().getDatabaseConnectionString();
    if (!database_manager_->connect(db_conn))
    {
        Logger::error("Failed to connect to database");
        return false;
    }

    lua_manager_ = std::make_unique<LuaManager>();
    if (!lua_manager_->initialize(this))
    {
        Logger::error("Failed to initialize LuaManager");
        return false;
    }

    world_ = std::make_unique<World>();
    anti_cheat_ = std::make_unique<AntiCheat>();

    Logger::info("Server initialized successfully");
    Logger::info("Tick rate: " + std::to_string(Config::getInstance().getTickRate()) + " Hz");

    return true;
}

void Server::run()
{
    running_ = true;

    auto last_time = std::chrono::high_resolution_clock::now();
    auto last_report_time = last_time;

    float target_fps = static_cast<float>(Config::getInstance().getTickRate());
    const auto frame_duration = std::chrono::microseconds(
        static_cast<long long>(1000000.0f / target_fps));

    Logger::info("Server main loop started");

    while (running_)
    {
        PerformanceMonitor::getInstance().startFrame();

        auto current_time = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            current_time - last_time);
        float delta_time = elapsed.count() / 1000000.0f;

        processEvents();
        update(delta_time);

        PerformanceMonitor::getInstance().endFrame();
        PerformanceMonitor::getInstance().setConnectedPlayers(players_.size());

        // Performance report a cada 60 segundos
        auto report_elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - last_report_time);
        if (report_elapsed.count() >= 60)
        {
            PerformanceMonitor::getInstance().printReport();
            last_report_time = current_time;
        }

        // Sleep para manter FPS consistente
        auto frame_time = std::chrono::high_resolution_clock::now() - current_time;
        if (frame_time < frame_duration)
        {
            std::this_thread::sleep_for(frame_duration - frame_time);
        }

        last_time = current_time;
    }

    Logger::info("Server main loop ended");
}

void Server::processEvents()
{
    auto packets = network_manager_->pollEvents(1);

    for (const auto &packet : packets)
    {
        PerformanceMonitor::getInstance().recordPacketReceived();

        switch (packet.type)
        {
        case PacketType::CONNECT:
            Logger::info("Client connected: " + std::to_string(packet.peer_id));
            break;

        case PacketType::DISCONNECT:
        {
            Logger::info("Client disconnected: " + std::to_string(packet.peer_id));
            std::lock_guard<std::mutex> lock(players_mutex_);

            auto it = players_.find(packet.peer_id);
            if (it != players_.end())
            {
                world_->removePlayer(packet.peer_id);
                players_.erase(it);
            }
            break;
        }

        case PacketType::AUTH_REQUEST:
            lua_manager_->callFunction("handle_auth_request", packet.peer_id, packet.data);
            break;

        case PacketType::PLAYER_MOVE:
        {
            if (players_.count(packet.peer_id))
            {
                auto player = players_[packet.peer_id];

                // Garante que temos bytes suficientes
                if (packet.data.size() < sizeof(float) * 3)
                {
                    Logger::error("Pacote PLAYER_MOVE inválido (tamanho insuficiente)");
                    break;
                }

                // Faz parsing binário direto
                const float *coords = reinterpret_cast<const float *>(packet.data.data());
                Vector3 new_pos{coords[0], coords[1], coords[2]};

                Vector3 old_pos = player->getPosition();

                // Anti-cheat
                if (Config::getInstance().isAntiCheatEnabled())
                {
                    if (!anti_cheat_->validateMovement(
                            packet.peer_id,
                            old_pos.x, old_pos.z,
                            new_pos.x, new_pos.z,
                            0.016f))
                    {
                        if (anti_cheat_->shouldBanPlayer(packet.peer_id))
                        {
                            Logger::error("Banning player " + std::to_string(packet.peer_id) + " for cheating");
                            network_manager_->disconnectPeer(packet.peer_id);
                            break;
                        }
                    }
                }

                player->setPosition(new_pos);

                // Passa pacote bruto pro Lua
                lua_manager_->callFunction("handle_player_move", packet.peer_id, packet.data);
            }
            break;
        }

        case PacketType::PLAYER_ACTION:
            if (anti_cheat_->validatePlayerAction(packet.peer_id, "action"))
            {
                lua_manager_->callFunction("handle_player_action", packet.peer_id, packet);
            }
            break;

        case PacketType::CHAT_MESSAGE:
            lua_manager_->callFunction("handle_chat_message", packet.peer_id, packet);
            break;

        case PacketType::NETWORK_COMMAND_REMOTE_CALL:
        {
            network_manager_->getRPCHandler().processGodotPacket(packet.peer_id, packet.data);
            break;
        }

        default:
            Logger::warning("Unknown packet type received: " +
                            std::to_string(static_cast<int>(packet.type)));
            break;
        }
    }
}

void Server::update(float delta_time)
{
    // Atualiza mundo
    world_->update(delta_time);

    // Atualiza lógica Lua
    lua_manager_->callFunction("update_world", delta_time);

    // Broadcast state para clientes (a cada 50ms)
    static float accumulator = 0.0f;
    accumulator += delta_time;

    if (accumulator >= 0.05f)
    {
        nlohmann::json world_state;
        world_state["players"] = nlohmann::json::array();

        std::lock_guard<std::mutex> lock(players_mutex_);
        for (const auto &[id, player] : players_)
        {
            world_state["players"].push_back(player->toJson());
        }

        std::string json_str = world_state.dump();
        std::vector<uint8_t> data(json_str.begin(), json_str.end());

        if (network_manager_->broadcastPacket(PacketType::WORLD_STATE, data, 0))
        {
            PerformanceMonitor::getInstance().recordPacketSent();
        }

        accumulator = 0.0f;
    }

    // Persiste posições no DB a cada 5 segundos
    static float db_accumulator = 0.0f;
    db_accumulator += delta_time;

    if (db_accumulator >= 5.0f)
    {
        savePlayerStates();
        db_accumulator = 0.0f;
    }
}

void Server::savePlayerStates()
{
    std::lock_guard<std::mutex> lock(players_mutex_);

    for (const auto &[id, player] : players_)
    {
        const auto &pos = player->getPosition();

        auto future = database_manager_->updatePlayerPositionAsync(
            player->getDbId(), pos.x, pos.y, pos.z);

        // Não aguarda o resultado - fire and forget
    }
}

void Server::shutdown()
{
    if (running_)
    {
        running_ = false;
        Logger::info("Shutting down server...");

        // Salva estados finais
        savePlayerStates();

        // Aguarda 1 segundo para operações assíncronas
        std::this_thread::sleep_for(std::chrono::seconds(1));

        network_manager_->shutdown();
        database_manager_->disconnect();

        enet_deinitialize();

        PerformanceMonitor::getInstance().printReport();
        Logger::info("Server shutdown complete");
    }
}