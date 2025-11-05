// include/server/World.h
#pragma once

#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <shared_mutex>

class Player;

// Spatial partitioning para otimizar queries espaciais
class SpatialGrid {
public:
    SpatialGrid(float cell_size = 50.0f);
    
    void insertPlayer(uint32_t player_id, float x, float z);
    void removePlayer(uint32_t player_id);
    void updatePlayer(uint32_t player_id, float x, float z);
    
    std::vector<uint32_t> queryRadius(float x, float z, float radius);
    std::vector<uint32_t> queryArea(float min_x, float min_z, float max_x, float max_z);

private:
    struct Cell {
        int x, z;
        
        bool operator==(const Cell& other) const {
            return x == other.x && z == other.z;
        }
    };
    
    struct CellHash {
        size_t operator()(const Cell& cell) const {
            return std::hash<int>()(cell.x) ^ (std::hash<int>()(cell.z) << 1);
        }
    };
    
    Cell getCell(float x, float z) const;
    
    float cell_size_;
    std::unordered_map<Cell, std::vector<uint32_t>, CellHash> grid_;
    std::unordered_map<uint32_t, Cell> player_to_cell_;
    mutable std::shared_mutex mutex_;
};

class World {
public:
    World();
    
    void update(float delta_time);
    
    void addPlayer(std::shared_ptr<Player> player);
    void removePlayer(uint32_t player_id);
    
    std::vector<std::shared_ptr<Player>> getPlayersInRadius(float x, float z, float radius);
    
    SpatialGrid* getSpatialGrid() { return &spatial_grid_; }

private:
    SpatialGrid spatial_grid_;
    std::unordered_map<uint32_t, std::shared_ptr<Player>> players_;
    mutable std::shared_mutex players_mutex_;
};