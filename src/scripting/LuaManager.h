// include/scripting/LuaManager.h
#pragma once

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>
#include <string>
#include <vector>
#include "utils/Logger.h"

class Server;

class LuaManager {
public:
    LuaManager();
    ~LuaManager();

    bool initialize(Server* server);
    bool loadScript(const std::string& filepath);
    
    // Call Lua functions
    template<typename... Args>
    void callFunction(const std::string& func_name, Args&&... args) {
        try {
            sol::protected_function func = lua_[func_name];
            if (func.valid()) {
                auto result = func(std::forward<Args>(args)...);
                if (!result.valid()) {
                    sol::error err = result;
                    Logger::error("Lua error in " + func_name + ": " + err.what());
                }
            }
        } catch (const std::exception& e) {
            Logger::error("Exception calling Lua function " + func_name + ": " + e.what());
        }
    }
    
    sol::state& getState() { return lua_; }

private:
    void registerBindings(Server* server);
    
    sol::state lua_;
};