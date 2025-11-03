/**
 * @file main.cpp
 * @brief Ponto de entrada do servidor multiplayer seguro
 *
 * Este arquivo contém a função principal que inicializa e gerencia
 * o ciclo de vida do servidor. As responsabilidades incluem:
 * - Configuração do tratamento de sinais para shutdown gracioso
 * - Inicialização do console para suporte a UTF-8
 * - Criação e inicialização da instância do servidor
 * - Execução do loop principal do servidor
 * - Tratamento de erros e finalização segura
 *
 * @author Sistema de Servidor Seguro
 * @version 1.0
 * @date 2025
 */

#include "server/server.h"
#include "utils/console_utils.h"
#include <iostream>
#include <csignal>

// Flag global para controle de shutdown
volatile std::sig_atomic_t g_running = 1;  ///< Controla se o servidor deve continuar rodando

/**
 * @brief Handler de sinal para shutdown gracioso
 *
 * Esta função é chamada quando o servidor recebe sinais de interrupção
 * (SIGINT - Ctrl+C) ou terminação (SIGTERM). Ela define a flag global
 * para parar o loop principal e exibe uma mensagem de desligamento.
 *
 * @param signal O sinal recebido (SIGINT ou SIGTERM)
 */
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = 0;
        std::cout << "\nRecebido sinal de shutdown, desligando servidor..." << std::endl;
    }
}

/**
 * @brief Função principal do servidor
 *
 * Esta é a função de entrada do aplicativo servidor. Ela gerencia
 * todo o ciclo de vida do servidor:
 *
 * 1. Configuração do tratamento de sinais para permitir shutdown gracioso
 * 2. Configuração do console para suporte a caracteres UTF-8
 * 3. Criação da instância do servidor
 * 4. Inicialização do servidor e seus componentes
 * 5. Execução do loop principal enquanto o servidor estiver rodando
 * 6. Finalização segura do servidor e seus recursos
 *
 * @return Retorna 0 em caso de sucesso, 1 em caso de erro
 */
int main() {
    try {
        // Configurar handler de sinal para permitir shutdown gracioso
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);
        
        // Configurar encoding do console para suportar caracteres especiais
        ConsoleUtils::setupConsoleEncoding();
        
        // Criar instância do servidor
        Server server;
        
        // Inicializar servidor e todos os seus componentes
        if (!server.initialize()) {
            std::cerr << "Falha ao inicializar servidor" << std::endl;
            return 1;
        }
        
        // Rodar servidor enquanto estiver ativo e não receber sinal de shutdown
        while (g_running && server.isRunning()) {
            server.run();
        }
        
        // Realizar shutdown seguro do servidor e todos os seus componentes
        server.shutdown();
        
        std::cout << "Servidor desligado com sucesso" << std::endl;
        
        system("pause");

        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Erro fatal: " << e.what() << std::endl;
        return 1;
    }
    catch (...) {
        std::cerr << "Erro fatal desconhecido!" << std::endl;
        return 1;
    }
}
