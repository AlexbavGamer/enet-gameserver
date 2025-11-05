// include/utils/CryptoUtils.h
#pragma once

#include <string>
#include <openssl/sha.h>
#include <openssl/rand.h>

class CryptoUtils {
public:
    // SHA-256 hash (para senhas - use com salt)
    static std::string sha256(const std::string& input);
    
    // Gera salt aleatório
    static std::string generateSalt(size_t length = 16);
    
    // Hash de senha com salt
    static std::string hashPassword(const std::string& password, const std::string& salt);
    
    // Verifica senha
    static bool verifyPassword(const std::string& password, const std::string& hash, const std::string& salt);
    
    // Gera token de sessão
    static std::string generateSessionToken();
};