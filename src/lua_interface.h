#pragma once
#include <sol/sol.hpp>
#include <iostream>
#include <string>
#include <unordered_map>
#include <memory>
#include "encoding_utils.h"

// Estrutura para armazenar estado Lua
struct LuaState {
    sol::state lua;
    // guardar environment por script
    std::unordered_map<std::string, sol::environment> scripts;

    LuaState();
    bool loadScript(const std::string& name, const std::string& code);
    bool executeFunction(const std::string& scriptName, const std::string& funcName, sol::variadic_args args);
    void registerAPI();
};

// Gerenciador global de estados Lua
extern std::unique_ptr<LuaState> luaManager;

// Funções principais para Lua
bool initLua();
void shutdownLua();
bool loadLuaScript(const std::string& name, const std::string& filepath);

// Template function implementation (declarado aqui, definido em cpp)
template<typename... Args>
bool callLuaFunction(const std::string& scriptName, const std::string& funcName, Args... args);


template<typename... Args>
bool callLuaFunction(const std::string& scriptName, const std::string& funcName, Args... args) {
    if (!luaManager) {
        safePrint("[LUA ERRO] Sistema Lua não inicializado");
        return false;
    }

    try {
        // Primeiro verifica se o namespace/table do script existe
        sol::object script_obj = luaManager->lua[scriptName];
        if (!script_obj.valid() || script_obj.get_type() != sol::type::table) {
            safePrint("[LUA ERRO] Script '" + scriptName + "' não encontrado em lua.globals()");
            return false;
        }

        sol::table script_table = script_obj.as<sol::table>();
        sol::object func_obj = script_table[funcName];

        if (!func_obj.valid() || func_obj.get_type() != sol::type::function) {
            safePrint("[LUA ERRO] Função '" + funcName + "' não encontrada no script '" + scriptName + "'");
            // listar funções disponíveis
            safePrint("[LUA DEBUG] Funções disponíveis em '" + scriptName + "':");
            for (auto&& pair : script_table) {
                if (pair.second.get_type() == sol::type::function) {
                    safePrint("[LUA DEBUG]  - " + pair.first.as<std::string>());
                }
            }
            return false;
        }

        sol::protected_function func = func_obj.as<sol::function>();

        safePrint("[LUA DEBUG] Executando função '" + funcName + "' no script '" + scriptName + "' via template");

        sol::protected_function_result result = func(args...);
        if (!result.valid()) {
            sol::error err = result;
            safePrint("[LUA ERRO] Erro ao executar função '" + funcName + "': " + std::string(err.what()));
            // tentar pegar traceback se disponível
            if (luaManager->lua["debug"] && luaManager->lua["debug"]["traceback"]) {
                sol::protected_function traceback = luaManager->lua["debug"]["traceback"];
                sol::protected_function_result tb_res = traceback();
                if (tb_res.valid()) {
                    safePrint("[LUA DEBUG] Stack trace: " + tb_res.get<std::string>());
                }
            }
            return false;
        }

        safePrint("[LUA DEBUG] Função '" + funcName + "' executada com sucesso via template");
        return true;
    } catch (const std::exception& e) {
        safePrint(std::string("[LUA ERRO] Exceção ao executar função '") + funcName + "': " + e.what());
        return false;
    }
}