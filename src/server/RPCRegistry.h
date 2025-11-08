// RPCRegistry.h
#ifndef RPC_REGISTRY_H
#define RPC_REGISTRY_H

#include "server/RPCHandler.h"
#include "utils/Logger.h"
#include <functional>
#include <string>
#include <vector>
#include <utility>

// ========== Template helpers FORA da classe ==========

// Template genérico
template<typename T>
T get_arg_helper(const Variant& v);

// Especializações
template<> inline float get_arg_helper<float>(const Variant& v) { 
    return static_cast<float>(v.f); 
}

template<> inline double get_arg_helper<double>(const Variant& v) { 
    return v.f; 
}

template<> inline int get_arg_helper<int>(const Variant& v) { 
    return static_cast<int>(v.i); 
}

template<> inline int64_t get_arg_helper<int64_t>(const Variant& v) { 
    return v.i; 
}

template<> inline bool get_arg_helper<bool>(const Variant& v) { 
    return v.b; 
}

template<> inline std::string get_arg_helper<std::string>(const Variant& v) { 
    return v.s; 
}

template<> inline Vector3 get_arg_helper<Vector3>(const Variant& v) { 
    return v.v3; 
}

template<> inline std::vector<Variant> get_arg_helper<std::vector<Variant>>(const Variant& v) {
    return v.arr;
}

template<> inline std::unordered_map<std::string, Variant> get_arg_helper<std::unordered_map<std::string, Variant>>(const Variant& v) {
    return v.dict;
}

// ========== Classe RPCRegistry ==========

class RPCRegistry {
private:
    RPCHandler& handler_;

public:
    explicit RPCRegistry(RPCHandler& handler) : handler_(handler) {}

    // ========== Registro Genérico (aceita qualquer argumento) ==========
    
    void registerRPC(const std::string& method_name, 
                     std::function<void(uint32_t, const std::vector<Variant>&)> callback) {
        handler_.registerRPCCallback(method_name, callback);
    }

    void registerRPCWithId(uint16_t id, const std::string& method_name,
                          std::function<void(uint32_t, const std::vector<Variant>&)> callback) {
        handler_.registerRPCCallbackWithId(id, method_name, callback);
    }

    // ========== Registro Type-Safe (tipos específicos) ==========
    
    template<typename... Args>
    void registerRPCTyped(const std::string& method_name, 
                          std::function<void(uint32_t, Args...)> callback) {
        registerRPC(method_name, 
            [callback, method_name](uint32_t peer_id, const std::vector<Variant>& args) {
                constexpr size_t expected = sizeof...(Args);
                if (args.size() != expected) {
                    Logger::error("RPC '" + method_name + "' expected " + 
                                std::to_string(expected) + " args, got " + 
                                std::to_string(args.size()));
                    return;
                }
                call_with_args<Args...>(callback, peer_id, args, std::index_sequence_for<Args...>{});
            }
        );
    }

    template<typename... Args>
    void registerRPCTypedWithId(uint16_t id, const std::string& method_name,
                               std::function<void(uint32_t, Args...)> callback) {
        registerRPCWithId(id, method_name,
            [callback, method_name](uint32_t peer_id, const std::vector<Variant>& args) {
                constexpr size_t expected = sizeof...(Args);
                if (args.size() != expected) {
                    Logger::error("RPC '" + method_name + "' expected " + 
                                std::to_string(expected) + " args, got " + 
                                std::to_string(args.size()));
                    return;
                }
                call_with_args<Args...>(callback, peer_id, args, std::index_sequence_for<Args...>{});
            }
        );
    }

    // ========== Helpers ==========
    
    // Lista todos os RPCs registrados
    void listRPCs() const {
        handler_.listRegisteredRPCs();
    }

    // Acesso ao handler subjacente
    RPCHandler& getHandler() { return handler_; }
    const RPCHandler& getHandler() const { return handler_; }

private:
    // Helper para expandir argumentos variádicos
    template<typename... Args, size_t... Is>
    static void call_with_args(std::function<void(uint32_t, Args...)>& func,
                               uint32_t peer_id,
                               const std::vector<Variant>& args,
                               std::index_sequence<Is...>) {
        func(peer_id, get_arg_helper<Args>(args[Is])...);
    }
};

#endif // RPC_REGISTRY_H