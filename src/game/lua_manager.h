#ifndef LUA_MANAGER_H
#define LUA_MANAGER_H

#include "../config/constants.h"
#include "../utils/logger.h"
#include "../server/server.h"
#include <sol/sol.hpp>
#include <string>
#include <unordered_map>
#include <memory>

class LuaManager {
private:
    std::unique_ptr<sol::state> lua_;
    std::unordered_map<std::string, std::string> scripts_;
    Server* server_;
    
public:
    LuaManager(Server* server = nullptr);
    ~LuaManager() = default;
    
    // Inicialização e shutdown
    bool initialize();
    void shutdown();
    
    // Carregamento de scripts
    bool loadScript(const std::string& script_name, const std::string& file_path);
    bool loadAllScripts(const std::string& scripts_directory);
    
    // Execução de funções
    bool callFunction(const std::string& function_name, sol::variadic_args args);
    bool callFunctionInScript(const std::string& script_name, const std::string& function_name, sol::variadic_args args);
    
    // Verificação de scripts
    bool isScriptLoaded(const std::string& script_name) const;
    bool isFunctionAvailable(const std::string& script_name, const std::string& function_name) const;
    
    // Acesso ao estado Lua
    sol::state& getLuaState() { return *lua_; }
    const sol::state& getLuaState() const { return *lua_; }
    
    // Gerenciamento do ponteiro do Server
    void setServer(Server* server) { server_ = server; }
    Server* getServer() const { return server_; }
    
    // Gerenciamento de scripts
    const std::unordered_map<std::string, std::string>& getLoadedScripts() const { return scripts_; }
    void listLoadedScripts() const;
    
private:
    // Validação de caminhos
    bool isValidScriptPath(const std::string& file_path) const;
    
    // Carregamento individual de script
    bool loadIndividualScript(const std::string& script_name, const std::string& file_path);
    
    // Binding de funções Lua
    void registerLuaFunctions();
};

// Instância global para compatibilidade com o código existente
extern std::unique_ptr<LuaManager> luaManager;

// Funções utilitárias para compatibilidade
bool initLua();
void shutdownLua();
bool loadLuaScript(const std::string& script_name, const std::string& file_path);
bool callLuaFunction(const std::string& script_name, const std::string& function_name, sol::variadic_args args);

#endif // LUA_MANAGER_H