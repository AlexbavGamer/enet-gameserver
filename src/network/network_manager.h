#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "../config/constants.h"
#include "../utils/logger.h"
#include <enet/enet.h>
#include <memory>

class NetworkManager {
private:
    ENetHost* server_;
    bool initialized_;
    
public:
    NetworkManager();
    ~NetworkManager();
    
    // Inicialização e shutdown
    bool initialize();
    void shutdown();
    
    // Verificação de status
    bool isInitialized() const { return initialized_; }
    
    // Processamento de eventos
    int service(ENetEvent* event, int timeout_ms);
    
    // Criação de servidor
    bool createServer(int port, int max_clients, int max_channels);
    
    // Envio de pacotes
    static void sendPacket(ENetPeer* peer, std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)> packet);
    
    // Broadcast
    void broadcast(std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)> packet);
    void broadcastExcept(ENetPeer* peer, std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)> packet);
    
    // Acesso ao servidor
    ENetHost* getServer() const { return server_; }
    
private:
    // Configuração do endereço
    void setupAddress(ENetAddress* address, int port);
};

#endif // NETWORK_MANAGER_H