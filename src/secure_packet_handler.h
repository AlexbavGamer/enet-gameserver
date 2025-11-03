#pragma once
#include <enet/enet.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>
#include <iostream>
#include "encoding_utils.h"
#include "server_common.h"

using json = nlohmann::json;

class ManagedPacket {
private:
    ENetPacket* packet_;
    
public:
// Construtor que cria um pacote
    explicit ManagedPacket(const std::string& data, uint8_t channel = 0, uint32_t flags = ENET_PACKET_FLAG_RELIABLE)
        : packet_(nullptr) {
        try {
            create(data, channel, flags);
        } catch (const std::exception& e) {
            safePrint("[MEMÓRIA ERRO] Falha ao criar packet: " + std::string(e.what()));
            throw;
        }
    }
    
    // Construtor que recebe um pacote existente (assume ownership)
    explicit ManagedPacket(ENetPacket* packet) : packet_(packet) {}
    
    // Destrutor que libera o pacote
    ~ManagedPacket() {
        if (packet_) {
            safePrint("[MEMÓRIA] Destruindo packet " + std::to_string(reinterpret_cast<uintptr_t>(packet_)));
            enet_packet_destroy(packet_);
        }
    }
    
    // Desabilitar cópia
    ManagedPacket(const ManagedPacket&) = delete;
    ManagedPacket& operator=(const ManagedPacket&) = delete;
    
    // Permitir movimentação
    ManagedPacket(ManagedPacket&& other) noexcept : packet_(other.packet_) {
        other.packet_ = nullptr;
    }
    
    ManagedPacket& operator=(ManagedPacket&& other) noexcept {
        if (this != &other) {
            if (packet_) {
                enet_packet_destroy(packet_);
            }
            packet_ = other.packet_;
            other.packet_ = nullptr;
        }
        return *this;
    }
    
// Cria um novo pacote
    void create(const std::string& data, uint8_t channel = 0, uint32_t flags = ENET_PACKET_FLAG_RELIABLE) {
        if (packet_) {
            enet_packet_destroy(packet_);
        }
        packet_ = enet_packet_create(data.c_str(), data.size() + 1, flags);
        if (!packet_) {
            throw std::runtime_error("Failed to create packet");
        }
        safePrint("[MEMÓRIA] Criado packet " + std::to_string(reinterpret_cast<uintptr_t>(packet_)) + " com tamanho " + std::to_string(data.size() + 1));
    }
    
    // Obtém o ponteiro bruto (usar com cuidado)
    ENetPacket* get() const { return packet_; }
    
    // Verifica se o pacote é válido
    bool isValid() const { return packet_ != nullptr; }
    
    // Converte para string
    std::string toString() const {
        if (!packet_) return "";
        return std::string((char*)packet_->data, packet_->dataLength);
    }
};

// Gerenciador de pacotes seguro
class SecurePacketHandler {
public:
    // Função callback para processamento de pacotes
    using PacketCallback = std::function<void(ENetHost*, ENetEvent&, const json&)>;
    
    // Mapeamento de IDs para callbacks
    std::unordered_map<PacketID, PacketCallback> packet_handlers_;
    
    // Construtor
    SecurePacketHandler();
    
    // Parse de pacote com validação
    static json parse(ENetPacket* packet);
    
    // Cria pacote gerenciado (retorna smart pointer)
    static std::unique_ptr<ManagedPacket> create(PacketID id, const json& data = {});
    
// Envia pacote com gerenciamento automático
    static bool sendPacket(ENetPeer* peer, std::unique_ptr<ManagedPacket> packet, uint8_t channel = 0);
    
    // Método para transferir ownership do pacote para ENet
    static void transferOwnershipToENet(std::unique_ptr<ManagedPacket>& packet);
    
    // Registra handler para um tipo de pacote
    void registerHandler(PacketID id, PacketCallback callback);
    
    // Processa pacote recebido
    void processPacket(ENetHost* server, ENetEvent& event);
    
    // Validação de pacotes
    static bool validatePacket(const json& packet);
    
    // Broadcast seguro (exceto remetente)
    static void broadcastExcept(ENetHost* server, ENetPeer* sender, std::unique_ptr<ManagedPacket> packet);
    
    // Broadcast para todos
    static void broadcastAll(ENetHost* server, std::unique_ptr<ManagedPacket> packet);
    
private:
    // Validação de estrutura básica do pacote
    bool validateBasicStructure(const json& packet) const;
    
    // Validação de dados específicos por tipo de pacote
    bool validatePacketData(PacketID id, const json& data) const;
};

// Funções utilitárias
std::unique_ptr<ManagedPacket> createPingPacket();
std::unique_ptr<ManagedPacket> createLoginPacket(const std::string& username);
std::unique_ptr<ManagedPacket> createMovePacket(int playerId, float x, float y);
std::unique_ptr<ManagedPacket> createChatPacket(const std::string& username, const std::string& message);
std::unique_ptr<ManagedPacket> createSpawnPlayerPacket(int playerId, const std::string& username, float x, float y);
std::unique_ptr<ManagedPacket> createLogoutPacket(int playerId);