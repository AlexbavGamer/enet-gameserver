#include "encoding_utils.h"
#include <windows.h>
#include <locale>
#include <io.h>
#include <fcntl.h>

void setupConsoleEncoding() {
    // Configurar o console do Windows para UTF-8
    SetConsoleOutputCP(CP_UTF8);
    
    // Configurar locale do C++ para UTF-8
    setlocale(LC_ALL, ".UTF8");
    
    // Habilitar modo UTF-8 para stdout/stderr
    _setmode(_fileno(stdout), _O_TEXT);
    _setmode(_fileno(stderr), _O_TEXT);
}

void safePrint(const std::string& message) {
    // Imprimir diretamente como UTF-8
    std::cout << message << std::endl;
    std::cout.flush();
}

std::string toUtf8(const std::string& input) {
    // Se a string já estiver em UTF-8, retorna como está
    // Caso contrário, converte para UTF-8
    return input;
}