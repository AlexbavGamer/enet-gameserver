#pragma once

#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "utils/Structs.h"


class Player {
public:
    Player(uint32_t peer_id, uint64_t db_id, const std::string& username);
    
    // Getters/Setters
    uint32_t getPeerId() const { return peer_id_; }
    uint64_t getDbId() const { return db_id_; }
    const std::string& getUsername() const { return username_; }
    
    const Vector3& getPosition() const { return position_; }
    void setPosition(const Vector3& pos) { position_ = pos; }
    
    int getHealth() const { return health_; }
    void setHealth(int health) { health_ = health; }
    
    // Serialização
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& json);
    
private:
    uint32_t peer_id_;
    uint64_t db_id_;
    std::string username_;
    Vector3 position_;
    int health_;
    int level_;
};