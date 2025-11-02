#include "secure_packet_handler.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include "encoding_utils.h"

// Implementação do gerenciador de pacotes seguro
SecurePacketHandler::SecurePacketHandler() {
    // Não registrar handlers aqui - serão registrados em setupPacketHandlers()
}

json SecurePacketHandler::parse(ENetPacket* packet) {
    if (!packet) {
        safePrint("[PACKET ERRO] Packet nulo recebido");
        return {};
    }
    
    std::string data((char*)packet->data, packet->dataLength);
    try {
        json result = json::parse(data);
        
        // Validação básica
        if (!validatePacket(result)) {
            safePrint("[PACKET ERRO] Pacote inválido recebido");
            return {};
        }
        
        return result;
    } catch (const std::exception& e) {
        safePrint("[PACKET ERRO] Falha ao parsear JSON: " + std::string(e.what()));
        return {};
    }
}

std::unique_ptr<ManagedPacket> SecurePacketHandler::create(PacketID id, const json& data) {
    try {
        json packet = { {"id", id}, {"data", data} };
        std::string str = packet.dump();
        auto managed_packet = std::make_unique<ManagedPacket>(str);
        
        if (!managed_packet->isValid()) {
            safePrint("[PACKET ERRO] Falha ao criar packet ID " + std::to_string(static_cast<int>(id)));
            return nullptr;
        }
        
        safePrint("[PACKET] Criado packet ID " + std::to_string(static_cast<int>(id)) + ", tamanho: " + std::to_string(str.size() + 1));
        return managed_packet;
    } catch (const std::exception& e) {
        safePrint("[PACKET ERRO] Exceção ao criar packet: " + std::string(e.what()));
        return nullptr;
    }
}

bool SecurePacketHandler::sendPacket(ENetPeer* peer, std::unique_ptr<ManagedPacket> packet, uint8_t channel) {
    if (!peer || !packet || !packet->isValid()) {
        safePrint("[PACKET ERRO] Tentativa de enviar packet inválido");
        return false;
    }
    
    try {
        int result = enet_peer_send(peer, channel, packet->get());
        if (result == 0) {
            safePrint("[PACKET] Packet enviado com sucesso para peer " + std::to_string(reinterpret_cast<uintptr_t>(peer)));
            transferOwnershipToENet(packet);
            return true;
        } else {
            safePrint("[PACKET ERRO] Falha ao enviar packet: " + std::to_string(result));
            return false;
        }
    } catch (const std::exception& e) {
        safePrint("[PACKET ERRO] Exceção ao enviar packet: " + std::string(e.what()));
        return false;
    }
}

void SecurePacketHandler::registerHandler(PacketID id, PacketCallback callback) {
    packet_handlers_[id] = callback;
    safePrint("[PACKET] Handler registrado para ID " + std::to_string(static_cast<int>(id)));
}

void SecurePacketHandler::processPacket(ENetHost* server, ENetEvent& event) {
    try {
        json msg = parse(event.packet);
        enet_packet_destroy(event.packet); // Destruir packet original imediatamente
        event.packet = nullptr;

        if (!msg.contains("id") || !msg["id"].is_number_integer()) {
            safePrint("[PACKET ERRO] Pacote inválido: sem ID válido");
            return;
        }

        PacketID id = static_cast<PacketID>(msg["id"].get<int>());
        json data = msg.value("data", json::object());

        // Buscar handler registrado
        auto it = packet_handlers_.find(id);
        if (it != packet_handlers_.end()) {
            it->second(server, event, data);
        } else {
            safePrint("[PACKET ERRO] Nenhum handler registrado para ID " + std::to_string(static_cast<int>(id)));
        }
    } catch (const std::exception& e) {
        safePrint("[PACKET ERRO] Exceção ao processar pacote: " + std::string(e.what()));
        if (event.packet) {
            enet_packet_destroy(event.packet);
            event.packet = nullptr;
        }
    }
}

bool SecurePacketHandler::validatePacket(const json& packet) {
    if (!packet.is_object()) {
        return false;
    }
    
    if (!packet.contains("id") || !packet["id"].is_number_integer()) {
        return false;
    }
    
    // Validar ID do pacote
    PacketID id = static_cast<PacketID>(packet["id"].get<int>());
    if (id < PING || id > LUA_RESPONSE) {
        return false;
    }
    
    // Validar seção 'data' se existir
    if (packet.contains("data") && !packet["data"].is_object()) {
        return false;
    }
    
    return SecurePacketHandler().validateBasicStructure(packet);
}

bool SecurePacketHandler::validateBasicStructure(const json& packet) const {
    // Validação básica de estrutura
    if (!packet.is_object()) return false;
    if (!packet.contains("id") || !packet["id"].is_number_integer()) return false;
    
    // Validação de dados específicos por tipo
    PacketID id = static_cast<PacketID>(packet["id"].get<int>());
    if (packet.contains("data")) {
        return validatePacketData(id, packet["data"]);
    }
    
    return true;
}

bool SecurePacketHandler::validatePacketData(PacketID id, const json& data) const {
    switch (id) {
        case LOGIN:
            return data.contains("user") && data["user"].is_string();
        case MOVE:
            return data.contains("x") && data["x"].is_number() &&
                   data.contains("y") && data["y"].is_number();
        case CHAT:
            return data.contains("msg") && data["msg"].is_string();
        case SPAWN_PLAYER:
            return data.contains("id") && data["id"].is_number_integer() &&
                   data.contains("user") && data["user"].is_string() &&
                   data.contains("x") && data["x"].is_number() &&
                   data.contains("y") && data["y"].is_number();
        case LOGOUT:
            return data.contains("id") && data["id"].is_number_integer();
        case LUA_SCRIPT:
            return data.contains("script") && data["script"].is_string();
        case LUA_RESPONSE:
            return data.contains("result") && data["result"].is_string();
        case PING:
        default:
            return true; // Pacotes sem dados específicos
    }
}

void SecurePacketHandler::broadcastExcept(ENetHost* server, ENetPeer* sender, std::unique_ptr<ManagedPacket> packet) {
    if (!server || !packet || !packet->isValid()) {
        safePrint("[PACKET ERRO] Tentativa de broadcast inválido");
        return;
    }
    
    int sent_count = 0;
    std::vector<std::unique_ptr<ManagedPacket>> packets_to_send;
    
    // Criar cópias do pacote para cada peer
    for (ENetPeer* peer = server->peers; peer < &server->peers[server->peerCount]; ++peer) {
        if (peer->state == ENET_PEER_STATE_CONNECTED && peer != sender) {
            packets_to_send.push_back(create(static_cast<PacketID>(packet->get()->data[0]), json::parse(packet->toString())["data"]));
        }
    }
    
    // Enviar todos os pacotes
    for (auto& packet_to_send : packets_to_send) {
        if (packet_to_send && packet_to_send->isValid()) {
            for (ENetPeer* peer = server->peers; peer < &server->peers[server->peerCount]; ++peer) {
                if (peer->state == ENET_PEER_STATE_CONNECTED && peer != sender) {
                    if (enet_peer_send(peer, 0, packet_to_send->get()) == 0) {
                        sent_count++;
                        packet_to_send.release();
                        break; // Mover para próximo pacote
                    }
                }
            }
        }
    }
    
    if (sent_count > 0) {
        safePrint("[PACKET] Broadcast enviado para " + std::to_string(sent_count) + " peers");
    } else {
        safePrint("[PACKET ERRO] Nenhum peer válido para broadcast");
    }
}

void SecurePacketHandler::broadcastAll(ENetHost* server, std::unique_ptr<ManagedPacket> packet) {
    broadcastExcept(server, nullptr, std::move(packet));
}

void SecurePacketHandler::transferOwnershipToENet(std::unique_ptr<ManagedPacket>& packet) {
    if (packet && packet->isValid()) {
        packet.release(); // Transfer ownership to ENet
    }
}

// Funções utilitárias
std::unique_ptr<ManagedPacket> createPingPacket() {
    return SecurePacketHandler::create(PING);
}

std::unique_ptr<ManagedPacket> createLoginPacket(const std::string& username) {
    json data = { {"user", username} };
    return SecurePacketHandler::create(LOGIN, data);
}

std::unique_ptr<ManagedPacket> createMovePacket(int playerId, float x, float y) {
    json data = { {"id", playerId}, {"x", x}, {"y", y} };
    return SecurePacketHandler::create(MOVE, data);
}

std::unique_ptr<ManagedPacket> createChatPacket(const std::string& username, const std::string& message) {
    json data = { {"user", username}, {"msg", message} };
    return SecurePacketHandler::create(CHAT, data);
}

std::unique_ptr<ManagedPacket> createSpawnPlayerPacket(int playerId, const std::string& username, float x, float y) {
    json data = { {"id", playerId}, {"user", username}, {"x", x}, {"y", y} };
    return SecurePacketHandler::create(SPAWN_PLAYER, data);
}

std::unique_ptr<ManagedPacket> createLogoutPacket(int playerId) {
    json data = { {"id", playerId} };
    return SecurePacketHandler::create(LOGOUT, data);
}