#pragma once
#include <string>
#include <iostream>

// Configuração de encoding para Windows
void setupConsoleEncoding();

// Função segura para imprimir strings com acentos
void safePrint(const std::string& message);

// Função para converter string para UTF-8 se necessário
std::string toUtf8(const std::string& input);