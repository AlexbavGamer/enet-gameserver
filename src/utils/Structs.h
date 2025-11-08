#pragma once

#include "nlohmann/json.hpp"

struct Vector3 {
    float x, y, z;
    
    nlohmann::json toJson() const {
        return {{"x", x}, {"y", y}, {"z", z}};
    }
};

struct Vector2 {
    float x,y;

    nlohmann::json toJson() const {
        return {{"x", x}, {"y", y}};
    }
};