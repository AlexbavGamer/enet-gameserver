// src/main.cpp
#include "server/Server.h"
#include "utils/Logger.h"
#include "utils/Config.h"
#include <iostream>
#include <csignal>
#include <memory>

std::unique_ptr<Server> g_server;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Logger::info("Received shutdown signal");
        if (g_server) {
            g_server->shutdown();
        }
    }
}

int main(int argc, char* argv[]) {
    // Parse argumentos
    uint16_t port = 7777;
    size_t max_clients = 100;
    std::string db_conn = "host=localhost user=root password=admin dbname=gamedb";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--max-clients" && i + 1 < argc) {
            max_clients = std::stoull(argv[++i]);
        } else if (arg == "--db-conn" && i + 1 < argc) {
            db_conn = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  --port <port>           Server port (default: 7777)\n"
                      << "  --max-clients <num>     Max simultaneous clients (default: 100)\n"
                      << "  --db-conn <connection>  Database connection string\n"
                      << "  --help                  Show this help\n";
            return 0;
        }
    }
    
    // Inicializa logger
    Logger::initialize("server.log");
    Logger::info("=== Game Server Starting ===");
    Logger::info("Port: " + std::to_string(port));
    Logger::info("Max clients: " + std::to_string(max_clients));
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Cria e inicializa servidor
    g_server = std::make_unique<Server>(port, max_clients);
    
    if (!g_server->initialize()) {
        Logger::error("Failed to initialize server");
        return 1;
    }
    
    // Run main loop
    try {
        g_server->run();
    } catch (const std::exception& e) {
        Logger::error("Server crashed: " + std::string(e.what()));
        return 1;
    }
    
    Logger::info("=== Server Shutdown Complete ===");
    return 0;
}