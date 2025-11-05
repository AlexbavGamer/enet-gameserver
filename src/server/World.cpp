// src/server/World.cpp
#include "server/World.h"
#include "server/Player.h"
#include <algorithm>
#include <cmath>

// SpatialGrid implementation
SpatialGrid::SpatialGrid(float cell_size) : cell_size_(cell_size) {}

SpatialGrid::Cell SpatialGrid::getCell(float x, float z) const {
    return Cell{
        static_cast<int>(std::floor(x / cell_size_)),
        static_cast<int>(std::floor(z / cell_size_))
    };
}

void SpatialGrid::insertPlayer(uint32_t player_id, float x, float z) {
    std::unique_lock lock(mutex_);
    
    Cell cell = getCell(x, z);
    grid_[cell].push_back(player_id);
    player_to_cell_[player_id] = cell;
}

void SpatialGrid::removePlayer(uint32_t player_id) {
    std::unique_lock lock(mutex_);
    
    auto it = player_to_cell_.find(player_id);
    if (it != player_to_cell_.end()) {
        Cell& cell = it->second;
        auto& players = grid_[cell];
        
        players.erase(std::remove(players.begin(), players.end(), player_id), players.end());
        
        if (players.empty()) {
            grid_.erase(cell);
        }
        
        player_to_cell_.erase(it);
    }
}

void SpatialGrid::updatePlayer(uint32_t player_id, float x, float z) {
    std::unique_lock lock(mutex_);
    
    Cell new_cell = getCell(x, z);
    auto it = player_to_cell_.find(player_id);
    
    if (it != player_to_cell_.end()) {
        Cell& old_cell = it->second;
        
        if (!(old_cell == new_cell)) {
            // Remove da célula antiga
            auto& old_players = grid_[old_cell];
            old_players.erase(std::remove(old_players.begin(), old_players.end(), player_id), old_players.end());
            
            if (old_players.empty()) {
                grid_.erase(old_cell);
            }
            
            // Adiciona na nova célula
            grid_[new_cell].push_back(player_id);
            player_to_cell_[player_id] = new_cell;
        }
    }
}

std::vector<uint32_t> SpatialGrid::queryRadius(float x, float z, float radius) {
    std::shared_lock lock(mutex_);
    
    std::vector<uint32_t> result;
    
    // Calcula células que intersectam o raio
    int cell_radius = static_cast<int>(std::ceil(radius / cell_size_));
    Cell center = getCell(x, z);
    
    for (int dx = -cell_radius; dx <= cell_radius; ++dx) {
        for (int dz = -cell_radius; dz <= cell_radius; ++dz) {
            Cell cell{center.x + dx, center.z + dz};
            
            auto it = grid_.find(cell);
            if (it != grid_.end()) {
                result.insert(result.end(), it->second.begin(), it->second.end());
            }
        }
    }
    
    return result;
}

std::vector<uint32_t> SpatialGrid::queryArea(float min_x, float min_z, float max_x, float max_z) {
    std::shared_lock lock(mutex_);
    
    std::vector<uint32_t> result;
    
    Cell min_cell = getCell(min_x, min_z);
    Cell max_cell = getCell(max_x, max_z);
    
    for (int x = min_cell.x; x <= max_cell.x; ++x) {
        for (int z = min_cell.z; z <= max_cell.z; ++z) {
            Cell cell{x, z};
            
            auto it = grid_.find(cell);
            if (it != grid_.end()) {
                result.insert(result.end(), it->second.begin(), it->second.end());
            }
        }
    }
    
    return result;
}

// World implementation
World::World() : spatial_grid_(50.0f) {}

void World::update(float delta_time) {
    std::shared_lock lock(players_mutex_);
    
    // Atualiza spatial grid para todos os jogadores
    for (const auto& [id, player] : players_) {
        const auto& pos = player->getPosition();
        spatial_grid_.updatePlayer(id, pos.x, pos.z);
    }
}

void World::addPlayer(std::shared_ptr<Player> player) {
    std::unique_lock lock(players_mutex_);
    
    uint32_t id = player->getPeerId();
    players_[id] = player;
    
    const auto& pos = player->getPosition();
    spatial_grid_.insertPlayer(id, pos.x, pos.z);
}

void World::removePlayer(uint32_t player_id) {
    std::unique_lock lock(players_mutex_);
    
    players_.erase(player_id);
    spatial_grid_.removePlayer(player_id);
}

std::vector<std::shared_ptr<Player>> World::getPlayersInRadius(float x, float z, float radius) {
    std::vector<std::shared_ptr<Player>> result;
    
    auto player_ids = spatial_grid_.queryRadius(x, z, radius);
    
    std::shared_lock lock(players_mutex_);
    for (uint32_t id : player_ids) {
        auto it = players_.find(id);
        if (it != players_.end()) {
            result.push_back(it->second);
        }
    }
    
    return result;
}