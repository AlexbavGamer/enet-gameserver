// include/utils/Config.h
#pragma once

#include <string>
#include <nlohmann/json.hpp>

class Config {
public:
    static Config& getInstance();
    
    bool load(const std::string& filepath);
    
    // Server config
    uint16_t getPort() const { return config_["server"]["port"]; }
    size_t getMaxClients() const { return config_["server"]["max_clients"]; }
    int getTickRate() const { return config_["server"]["tick_rate"]; }
    
    // Database config
    std::string getDatabaseConnectionString() const;
    int getDatabasePoolSize() const { return config_["database"]["pool_size"]; }
    
    // Game config
    float getWorldSize() const { return config_["game"]["world_size"]; }
    float getSpatialGridCellSize() const { return config_["game"]["spatial_grid_cell_size"]; }
    
    // Security config
    int getRateLimitPerSecond() const { return config_["security"]["rate_limit_per_second"]; }
    int getMaxLoginAttempts() const { return config_["security"]["max_login_attempts"]; }
    bool isAntiCheatEnabled() const { return config_["security"]["enable_anti_cheat"]; }
    
    // Logging config
    std::string getLogLevel() const { return config_["logging"]["level"]; }
    std::string getLogFile() const { return config_["logging"]["file"]; }
    bool isConsoleOutputEnabled() const { return config_["logging"]["console_output"]; }
    
    const nlohmann::json& getRaw() const { return config_; }

private:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    nlohmann::json config_;
};