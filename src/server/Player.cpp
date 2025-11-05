#include "Player.h"
#include "utils/Logger.h"
#include <nlohmann/json.hpp>

Player::Player(uint32_t peer_id, uint64_t db_id, const std::string& username)
    : peer_id_(peer_id)
    , db_id_(db_id)
    , username_(username)
    , health_(100)
    , level_(1) {
    // Inicializa posição com valores padrão
    position_ = Vector3{0.0f, 0.0f, 0.0f};
}

nlohmann::json Player::toJson() const {
    nlohmann::json json_data;
    json_data["peer_id"] = peer_id_;
    json_data["db_id"] = db_id_;
    json_data["username"] = username_;
    json_data["position"] = position_.toJson();
    json_data["health"] = health_;
    json_data["level"] = level_;
    return json_data;
}

void Player::fromJson(const nlohmann::json& json) {
    if (json.contains("peer_id")) {
        peer_id_ = json["peer_id"].get<uint32_t>();
    }
    if (json.contains("db_id")) {
        db_id_ = json["db_id"].get<uint64_t>();
    }
    if (json.contains("username")) {
        username_ = json["username"].get<std::string>();
    }
    if (json.contains("position")) {
        position_.x = json["position"]["x"].get<float>();
        position_.y = json["position"]["y"].get<float>();
        position_.z = json["position"]["z"].get<float>();
    }
    if (json.contains("health")) {
        health_ = json["health"].get<int>();
    }
    if (json.contains("level")) {
        level_ = json["level"].get<int>();
    }
}