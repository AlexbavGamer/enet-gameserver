#include "console_utils.h"
#include <windows.h>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace ConsoleUtils {

void setupConsoleEncoding() {
    // Configurar o console para usar UTF-8
    SetConsoleOutputCP(CP_UTF8);
    
    // Configurar o console para usar o charset correto
    SetConsoleCP(CP_UTF8);
}

void clearScreen() {
    // Limpa a tela do console
    system("cls");
}

void pause() {
    // Pausa a execução e aguarda input do usuário
    system("pause");
}

std::string formatLogMessage(const std::string& message) {
    // Adiciona timestamp à mensagem de log
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << " - " << message;
    
    return ss.str();
}

std::string toLowercase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

bool startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.length() > str.length()) {
        return false;
    }
    
    return str.substr(0, prefix.length()) == prefix;
}

bool endsWith(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) {
        return false;
    }
    
    return str.substr(str.length() - suffix.length()) == suffix;
}

} // namespace ConsoleUtils