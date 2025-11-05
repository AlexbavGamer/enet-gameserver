// src/utils/CryptoUtils.cpp
#include "utils/CryptoUtils.h"
#include <sstream>
#include <iomanip>
#include <vector>

std::string CryptoUtils::sha256(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    
    return ss.str();
}

std::string CryptoUtils::generateSalt(size_t length) {
    std::vector<unsigned char> salt(length);
    RAND_bytes(salt.data(), static_cast<int>(length));
    
    std::stringstream ss;
    for (unsigned char byte : salt) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    
    return ss.str();
}

std::string CryptoUtils::hashPassword(const std::string& password, const std::string& salt) {
    return sha256(password + salt);
}

bool CryptoUtils::verifyPassword(const std::string& password, const std::string& hash, const std::string& salt) {
    return hashPassword(password, salt) == hash;
}

std::string CryptoUtils::generateSessionToken() {
    return generateSalt(32);
}