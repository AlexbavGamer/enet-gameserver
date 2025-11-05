// src/server/AntiCheat.cpp
#include "server/AntiCheat.h"
#include "utils/Logger.h"
#include <cmath>

AntiCheat::AntiCheat() {}

bool AntiCheat::validatePlayerAction(uint32_t player_id, const std::string& action_type) {
    auto& behavior = player_behaviors_[player_id];
    behavior.player_id = player_id;
    
    auto now = std::chrono::steady_clock::now();
    behavior.action_timestamps.push_back(now);
    
    // Remove timestamps antigos (mais de 1 segundo)
    auto cutoff = now - std::chrono::seconds(1);
    behavior.action_timestamps.erase(
        std::remove_if(behavior.action_timestamps.begin(), 
                      behavior.action_timestamps.end(),
                      [cutoff](const auto& ts) { return ts < cutoff; }),
        behavior.action_timestamps.end()
    );
    
    // Valida rate
    if (!validateActionRate(player_id)) {
        flagSuspiciousActivity(player_id, "Action rate exceeded: " + action_type);
        return false;
    }
    
    return true;
}

bool AntiCheat::validateMovement(uint32_t player_id, float old_x, float old_z, 
                                 float new_x, float new_z, float delta_time) {
    auto& behavior = player_behaviors_[player_id];
    
    // Calcula distância percorrida
    float dx = new_x - old_x;
    float dz = new_z - old_z;
    float distance = std::sqrt(dx * dx + dz * dz);
    
    // Calcula velocidade
    float speed = (delta_time > 0.0f) ? (distance / delta_time) : 0.0f;
    
    if (speed > MAX_SPEED) {
        flagSuspiciousActivity(player_id, 
            "Speed hack detected: " + std::to_string(speed) + " units/s");
        
        Logger::warning("Player " + std::to_string(player_id) + 
                       " moving too fast: " + std::to_string(speed) + " units/s");
        return false;
    }
    
    behavior.last_position_x = new_x;
    behavior.last_position_z = new_z;
    behavior.last_movement_time = std::chrono::steady_clock::now();
    
    return true;
}

bool AntiCheat::validateActionRate(uint32_t player_id) {
    auto& behavior = player_behaviors_[player_id];
    
    if (behavior.action_timestamps.size() > MAX_ACTIONS_PER_SECOND) {
        return false;
    }
    
    return true;
}

void AntiCheat::flagSuspiciousActivity(uint32_t player_id, const std::string& reason) {
    auto& behavior = player_behaviors_[player_id];
    behavior.suspicious_actions++;
    
    Logger::warning("Suspicious activity from player " + std::to_string(player_id) + 
                   ": " + reason + " (total: " + 
                   std::to_string(behavior.suspicious_actions) + ")");
}

bool AntiCheat::shouldBanPlayer(uint32_t player_id) {
    auto it = player_behaviors_.find(player_id);
    
    if (it != player_behaviors_.end()) {
        return it->second.suspicious_actions >= SUSPICIOUS_THRESHOLD;
    }
    
    return false;
}

void AntiCheat::cleanup() {
    // Remove dados de jogadores inativos (implementar lógica de cleanup)
}