#ifndef CONSOLE_UTILS_H
#define CONSOLE_UTILS_H

#include <string>

namespace ConsoleUtils {
    // Configura o encoding do console para suportar caracteres Unicode
    void setupConsoleEncoding();
    
    // Limpa a tela do console
    void clearScreen();
    
    // Pausa a execução e aguarda input do usuário
    void pause();
    
    // Formata uma mensagem de log com timestamp
    std::string formatLogMessage(const std::string& message);
    
    // Converte string para lowercase
    std::string toLowercase(const std::string& str);
    
    // Verifica se uma string começa com um prefixo
    bool startsWith(const std::string& str, const std::string& prefix);
    
    // Verifica se uma string termina com um sufixo
    bool endsWith(const std::string& str, const std::string& suffix);
}

#endif // CONSOLE_UTILS_H