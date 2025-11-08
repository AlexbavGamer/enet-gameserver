// src/utils/Config.cpp
#include "utils/Config.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>

Config& Config::getInstance() {
    static Config instance;
    return instance;
}

bool Config::load(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            Logger::error("Failed to open config file: " + filepath);
            return false;
        }
        
        file >> config_;
        Logger::info("Configuration loaded from: " + filepath);
        return true;
        
    } catch (const nlohmann::json::exception& e) {
        Logger::error("JSON parse error in config: " + std::string(e.what()));
        return false;
    }
}

std::string Config::getDatabaseConnectionString() const {
    std::stringstream ss;
    
    std::string db_type = config_["database"]["type"];

    if (db_type == "postgresql") {
        ss << "postgresql://"
           << "dbname=" << config_["database"]["name"].get<std::string>() << " "
           << "user=" << config_["database"]["user"].get<std::string>() << " ";

        if (config_["database"].contains("password") && !config_["database"]["password"].get<std::string>().empty()) {
            ss << "password=" << config_["database"]["password"].get<std::string>() << " ";
        }

        ss << "host=" << config_["database"]["host"].get<std::string>() << " "
           << "port=" << config_["database"]["port"].get<int>();

    } else if (db_type == "sqlite") {
        ss << "sqlite3://db=" << config_["database"]["name"].get<std::string>();

    } else if (db_type == "mysql") {
        ss << "mysql://"
           << "db=" << config_["database"]["name"].get<std::string>() << " "
           << "user=" << config_["database"]["user"].get<std::string>() << " ";

        if (config_["database"].contains("password") && !config_["database"]["password"].get<std::string>().empty()) {
            ss << "password=" << config_["database"]["password"].get<std::string>() << " ";
        }

        ss << "host=" << config_["database"]["host"].get<std::string>() << " "
           << "port=" << config_["database"]["port"].get<int>();
    }

    return ss.str();
}
