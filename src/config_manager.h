#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "server_common.h"

class ConfigManager {
private:
    ServerConfig config_;
    std::unordered_map<std::string, std::string> config_values_;
    
    // Default configuration values
    void setDefaults();
    
public:
    ConfigManager();
    ~ConfigManager() = default;
    
    // Load configuration from file
    bool loadFromFile(const std::string& filename);
    
    // Load configuration from environment variables
    void loadFromEnvironment();
    
    // Get configuration values
    ServerConfig getConfig() const { return config_; }
    
    // Get specific value
    std::string getValue(const std::string& key, const std::string& default_value = "") const;
    
    // Set specific value
    void setValue(const std::string& key, const std::string& value);
    
    // Save configuration to file
    bool saveToFile(const std::string& filename) const;
    
    // Validate configuration
    bool validate() const;
    
    // Get all configuration keys
    std::vector<std::string> getKeys() const;
    
    // Reset to defaults
    void resetToDefaults();
};