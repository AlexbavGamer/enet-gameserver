#include "network_manager.h"

NetworkManager::NetworkManager() : server_(nullptr), initialized_(false) {
}

NetworkManager::~NetworkManager() {
    shutdown();
}

bool NetworkManager::initialize() {
    if (initialized_) {
        LOG_WARNING(Config::LOG_PREFIX_DEBUG, "NetworkManager já inicializado");
        return true;
    }
    
    if (enet_initialize() != 0) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, Config::ENET_INIT_ERROR);
        return false;
    }
    
    // Registrar função de cleanup
    atexit(enet_deinitialize);
    
    initialized_ = true;
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "NetworkManager inicializado com sucesso");
    return true;
}

void NetworkManager::shutdown() {
    if (!initialized_) {
        return;
    }
    
    if (server_) {
        enet_host_destroy(server_);
        server_ = nullptr;
    }
    
    initialized_ = false;
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "NetworkManager finalizado");
}

bool NetworkManager::createServer(int port, int max_clients, int max_channels) {
    if (!initialized_) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "NetworkManager não inicializado");
        return false;
    }
    
    ENetAddress address;
    setupAddress(&address, port);
    
    server_ = enet_host_create(&address, max_clients, max_channels, 0, 0);
    if (!server_) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, Config::SERVER_CREATE_ERROR);
        return false;
    }
    
    LOG_INFO(Config::LOG_PREFIX_DEBUG, "Servidor criado na porta " + std::to_string(port));
    return true;
}

int NetworkManager::service(ENetEvent* event, int timeout_ms) {
    if (!server_) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Servidor não inicializado");
        return -1;
    }
    
    return enet_host_service(server_, event, timeout_ms);
}

void NetworkManager::sendPacket(ENetPeer* peer, std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)> packet) {
    if (!peer || !packet) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Tentativa de enviar pacote inválido");
        return;
    }
    
    enet_peer_send(peer, 0, packet.release());
}

void NetworkManager::broadcast(std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)> packet) {
    if (!server_ || !packet) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Tentativa de broadcast com pacote inválido");
        return;
    }
    
    enet_host_broadcast(server_, 0, packet.release());
}

void NetworkManager::broadcastExcept(ENetPeer* peer, std::unique_ptr<ENetPacket, decltype(&enet_packet_destroy)> packet) {
    if (!server_ || !packet) {
        LOG_ERROR(Config::LOG_PREFIX_ERROR, "Tentativa de broadcast com pacote inválido");
        return;
    }
    
    enet_host_broadcast(server_, 0, packet.release());
}

void NetworkManager::setupAddress(ENetAddress* address, int port) {
    address->host = ENET_HOST_ANY;
    address->port = port;
}