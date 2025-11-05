// include/server/AntiCheat.h
#pragma once

#include <unordered_map>
#include <chrono>
#include <vector>
#include <cstdint>

struct PlayerBehavior {
    uint32_t player_id;
    std::vector<std::chrono::steady_clock::time_point> action_timestamps;
    float last_position_x, last_position_z;
    std::chrono::steady_clock::time_point last_movement_time;
    int suspicious_actions;
};

class AntiCheat {
public:
    AntiCheat();
    
    // Valida ação de jogador
    bool validatePlayerAction(uint32_t player_id, const std::string& action_type);
    
    // Valida movimento
    bool validateMovement(uint32_t player_id, float old_x, float old_z, 
                         float new_x, float new_z, float delta_time);
    
    // Valida rate de ações
    bool validateActionRate(uint32_t player_id);
    
    // Marca jogador como suspeito
    void flagSuspiciousActivity(uint32_t player_id, const std::string& reason);
    
    // Verifica se jogador deve ser banido
    bool shouldBanPlayer(uint32_t player_id);
    
    void cleanup(); // Remove dados de jogadores desconectados

private:
    std::unordered_map<uint32_t, PlayerBehavior> player_behaviors_;
    
    // Limites configuráveis
    static constexpr float MAX_SPEED = 15.0f; // unidades por segundo
    static constexpr int MAX_ACTIONS_PER_SECOND = 20;
    static constexpr int SUSPICIOUS_THRESHOLD = 10;
};