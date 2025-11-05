// include/server/NetworkManager.h
#pragma once

#include <enet/enet.h>
#include <functional>
#include <vector>
#include <string>
#include <cstdint>

enum class PacketType : uint8_t {
    CONNECT = 0,
    DISCONNECT,
    AUTH_REQUEST,
    AUTH_RESPONSE,
    PLAYER_MOVE,
    PLAYER_ACTION,
    CHAT_MESSAGE,
    WORLD_STATE,
    RPC_CALL,
    BROADCAST
};

struct Packet {
    PacketType type;
    std::vector<uint8_t> data;
    uint32_t peer_id;
};

class NetworkManager {
public:
    NetworkManager(uint16_t port, size_t max_clients);
    ~NetworkManager();

    bool initialize();
    void shutdown();
    
    // Polling de eventos (call from main loop)
    std::vector<Packet> pollEvents(uint32_t timeout_ms = 0);
    
    // Envio de dados
    bool sendPacket(uint32_t peer_id, PacketType type, const std::vector<uint8_t>& data, bool reliable = true);
    bool broadcastPacket(PacketType type, const std::vector<uint8_t>& data, uint32_t exclude_peer = 0);
    
    // Gerenciamento de peers
    void disconnectPeer(uint32_t peer_id);
    size_t getConnectedPeerCount() const;
    
    ENetHost* getHost() const { return host_; }

private:
    ENetHost* host_;
    uint16_t port_;
    size_t max_clients_;
    
    // Mapeia ENetPeer* para IDs únicos
    std::unordered_map<ENetPeer*, uint32_t> peer_to_id_;
    std::unordered_map<uint32_t, ENetPeer*> id_to_peer_;
    uint32_t next_peer_id_;
};