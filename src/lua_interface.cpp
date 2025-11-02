#include "lua_interface.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "encoding_utils.h"

// Implementação do gerenciador Lua
std::unique_ptr<LuaState> luaManager;

LuaState::LuaState() {
    lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::string, sol::lib::table, sol::lib::math);
    registerAPI();
}

bool LuaState::loadScript(const std::string& name, const std::string& code) {
    try {
        // Cria environment isolado que cai back para globals
        sol::environment env(lua, sol::create, lua.globals());

        // Executa o script dentro do env
        sol::protected_function_result result = lua.safe_script(code, env, sol::script_pass_on_error);
        if (!result.valid()) {
            sol::error err = result;
            safePrint("[LUA ERRO] Falha ao carregar script '" + name + "': " + std::string(err.what()));
            return false;
        }

        // Expor o environment como lua.<scriptName> para compatibilidade com código existente
        // Converte env para sol::table e atribui em globals
        sol::table env_table = env;
        lua[name] = env_table;

        // Salva o environment no mapa para futuras chamadas diretas
        scripts.emplace(name, std::move(env));

        safePrint("[LUA] Script '" + name + "' carregado e registrado em lua['" + name + "'] com sucesso");
        return true;
    } catch (const std::exception& e) {
        safePrint("[LUA ERRO] Exceção ao carregar script '" + name + "': " + std::string(e.what()));
        return false;
    }
}
bool LuaState::executeFunction(const std::string& scriptName, const std::string& funcName, sol::variadic_args args) {
    auto it = scripts.find(scriptName);
    if (it == scripts.end()) {
        safePrint("[LUA ERRO] Script '" + scriptName + "' não encontrado");
        safePrint("[LUA DEBUG] Scripts disponíveis: ");
        for (const auto& script : scripts) {
            safePrint("[LUA DEBUG]  - " + script.first);
        }
        return false;
    }

    try {
        sol::protected_function func = lua[funcName];
        if (!func.valid()) {
            safePrint("[LUA ERRO] Função '" + funcName + "' não encontrada no script '" + scriptName + "'");
            safePrint("[LUA DEBUG] Script '" + scriptName + "' funções disponíveis: ");
            
            // Listar todas as funções globais no script
            sol::table script_table = lua[scriptName];
            if (script_table.valid()) {
                for (auto&& pair : script_table) {
                    if (pair.second.get_type() == sol::type::function) {
                        safePrint("[LUA DEBUG]  - " + pair.first.as<std::string>());
                    }
                }
            }
            return false;
        }

        safePrint("[LUA DEBUG] Executando função '" + funcName + "' no script '" + scriptName + "' com " + std::to_string(args.size()) + " argumentos");
        
        sol::protected_function_result result = func(args);
        if (!result.valid()) {
            sol::error err = result;
            std::string error_msg = std::string(err.what());
            safePrint("[LUA ERRO] Erro ao executar função '" + funcName + "': " + error_msg);
            safePrint("[LUA DEBUG] Script: " + scriptName);
            safePrint("[LUA DEBUG] Função: " + funcName);
            safePrint("[LUA DEBUG] Número de argumentos: " + std::to_string(args.size()));
            
            // Log de pilha se disponível
            if (lua["debug"] && lua["debug"]["traceback"]) {
                sol::protected_function traceback = lua["debug"]["traceback"];
                sol::protected_function_result traceback_result = traceback();
                if (traceback_result.valid()) {
                    safePrint("[LUA DEBUG] Stack trace: " + traceback_result.get<std::string>());
                }
            }
            return false;
        }

        safePrint("[LUA DEBUG] Função '" + funcName + "' executada com sucesso");
        return true;
    } catch (const std::exception& e) {
        std::string error_msg = std::string(e.what());
        safePrint("[LUA ERRO] Exceção ao executar função '" + funcName + "': " + error_msg);
        safePrint("[LUA DEBUG] Script: " + scriptName);
        safePrint("[LUA DEBUG] Função: " + funcName);
        safePrint("[LUA DEBUG] Tipo de exceção: " + std::string(typeid(e).name()));
        return false;
    }
}

void LuaState::registerAPI() {
    // Registrar funções utilitárias
    lua.set_function("log", [](const std::string& message) {
        safePrint("[LUA] " + message);
    });

    lua.set_function("print", [](const std::string& message) {
        safePrint("[LUA PRINT] " + message);
    });
}

// Funções globais
bool initLua() {
    try {
        luaManager = std::make_unique<LuaState>();
        safePrint("[LUA] Sistema Lua inicializado com sucesso");
        return true;
    } catch (const std::exception& e) {
        safePrint("[LUA ERRO] Falha ao inicializar Lua: " + std::string(e.what()));
        return false;
    }
}

void shutdownLua() {
    luaManager.reset();
    safePrint("[LUA] Sistema Lua finalizado");
}

bool loadLuaScript(const std::string& name, const std::string& filepath) {
    if (!luaManager) {
        safePrint("[LUA ERRO] Sistema Lua não inicializado");
        return false;
    }

    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            safePrint("[LUA ERRO] Não foi possível abrir arquivo: " + filepath);
            return false;
        }

        std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return luaManager->loadScript(name, code);
    } catch (const std::exception& e) {
        safePrint("[LUA ERRO] Erro ao carregar script '" + name + "': " + std::string(e.what()));
        return false;
    }
}
