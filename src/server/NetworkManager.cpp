// src/server/NetworkManager.cpp
#include "server/NetworkManager.h"
#include "utils/Logger.h"
#include <magic_enum/magic_enum.hpp>
#include "NetworkManager.h"

NetworkManager::NetworkManager(uint16_t port, size_t max_clients)
    : host_(nullptr), port_(port), max_clients_(max_clients), next_peer_id_(1), rpc_handler_() {}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize() {
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port_;
    
    // Bandwidth: 0 = unlimited
    host_ = enet_host_create(&address, max_clients_, 2, 0, 0);
    
    if (!host_) {
        Logger::error("Failed to create ENet host");
        return false;
    }
    
    Logger::info("NetworkManager initialized on port " + std::to_string(port_));
    return true;
}

void NetworkManager::shutdown() {
    if (host_) {
        enet_host_destroy(host_);
        host_ = nullptr;
    }
}

std::vector<Packet> NetworkManager::pollEvents(uint32_t timeout_ms) {
    std::vector<Packet> packets;
    ENetEvent event;
    
    while (enet_host_service(host_, &event, timeout_ms) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT: {
                uint32_t peer_id = next_peer_id_++;
                peer_to_id_[event.peer] = peer_id;
                id_to_peer_[peer_id] = event.peer;
                
                Packet pkt;
                pkt.type = PacketType::CONNECT;
                pkt.peer_id = peer_id;
                packets.push_back(pkt);
                break;
            }
            
            case ENET_EVENT_TYPE_DISCONNECT: {
                auto it = peer_to_id_.find(event.peer);
                if (it != peer_to_id_.end()) {
                    Packet pkt;
                    pkt.type = PacketType::DISCONNECT;
                    pkt.peer_id = it->second;
                    packets.push_back(pkt);
                    
                    id_to_peer_.erase(it->second);
                    peer_to_id_.erase(it);
                }
                break;
            }
            
            case ENET_EVENT_TYPE_RECEIVE: {
                auto it = peer_to_id_.find(event.peer);
                if (it != peer_to_id_.end()) {
                    Packet pkt;
                    pkt.peer_id = it->second;

                    // DETECTA GODOT RPCs
                    if (event.packet->dataLength > 0) {
                        uint8_t cmd = event.packet->data[0];
                        if (cmd == 0x20) {
                            pkt.type = PacketType::NETWORK_COMMAND_REMOTE_CALL;
                        } else {
                            pkt.type = static_cast<PacketType>(cmd);
                        }
                    } else {
                        pkt.type = PacketType::DISCONNECT; // fallback
                    }

                    pkt.data.assign(event.packet->data, event.packet->data + event.packet->dataLength);
                    packets.push_back(std::move(pkt));
                }
                enet_packet_destroy(event.packet);
                break;
            }

            default:
                break;
        }
    }
    
    return packets;
}

bool NetworkManager::sendPacket(uint32_t peer_id, PacketType type, 
                                const std::vector<uint8_t>& data, bool reliable) {
    auto it = id_to_peer_.find(peer_id);
    if (it == id_to_peer_.end()) {
        return false;
    }
    
    std::vector<uint8_t> packet_data;
    packet_data.push_back(static_cast<uint8_t>(type));
    packet_data.insert(packet_data.end(), data.begin(), data.end());
    
    uint32_t flags = reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED;
    ENetPacket* packet = enet_packet_create(packet_data.data(), packet_data.size(), flags);
    
    return enet_peer_send(it->second, 0, packet) == 0;
}

bool NetworkManager::broadcastPacket(PacketType type, const std::vector<uint8_t>& data, 
                                    uint32_t exclude_peer) {
    std::vector<uint8_t> packet_data;
    packet_data.push_back(static_cast<uint8_t>(type));
    packet_data.insert(packet_data.end(), data.begin(), data.end());
    
    ENetPacket* packet = enet_packet_create(packet_data.data(), packet_data.size(), 
                                            ENET_PACKET_FLAG_UNSEQUENCED);
    
    for (const auto& [id, peer] : id_to_peer_) {
        if (id != exclude_peer) {
            enet_peer_send(peer, 0, packet);
        }
    }
    
    return true;
}

// void NetworkManager::sendRPC(uint32_t peer_id, const std::string &node_path, const std::string &method, const std::vector<Variant> &args, bool reliable)
// {
// auto payload = rpc_handler_. buildGodotRPCPacket(node_path, method, args);
// std::vector<uint8_t> pkt;
// pkt.push_back(static_cast<uint8_t>(PacketType::RPC_CALL));
// pkt.insert(pkt.end(), payload.begin(), payload.end());
// sendPacket(peer_id, PacketType::RPC_CALL, std::vector<uint8_t>(payload.begin(), payload.end()), reliable);
// }

void NetworkManager::disconnectPeer(uint32_t peer_id) {
    auto it = id_to_peer_.find(peer_id);
    if (it != id_to_peer_.end()) {
        enet_peer_disconnect(it->second, 0);
    }
}

size_t NetworkManager::getConnectedPeerCount() const {
    return id_to_peer_.size();
}