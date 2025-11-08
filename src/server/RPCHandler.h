// RPCHandler.h
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include "utils/Structs.h"

#define NODE_ID_COMPRESSION_SHIFT 4
#define NAME_ID_COMPRESSION_SHIFT 6
#define BYTE_ONLY_OR_NO_ARGS_SHIFT 7

#define NODE_ID_COMPRESSION_FLAG ((1 << NODE_ID_COMPRESSION_SHIFT) | (1 << (NODE_ID_COMPRESSION_SHIFT + 1)))
#define NAME_ID_COMPRESSION_FLAG (1 << NAME_ID_COMPRESSION_SHIFT)
#define BYTE_ONLY_OR_NO_ARGS_FLAG (1 << BYTE_ONLY_OR_NO_ARGS_SHIFT)

struct Variant
{
    enum Type : uint8_t
    {
        NIL = 0,
        BOOL = 1,
        INT = 2,
        FLOAT = 3,
        STRING = 4,
        VECTOR3 = 5,
        ARRAY = 6,
        DICTIONARY = 7
    } type = NIL;

    bool b = false;
    int64_t i = 0;
    double f = 0.0;
    std::string s;
    Vector3 v3;
    std::vector<Variant> arr;
    std::unordered_map<std::string, Variant> dict;

    Variant() : type(NIL) {}
    Variant(bool val) : type(BOOL), b(val) {}
    Variant(int64_t val) : type(INT), i(val) {}
    Variant(double val) : type(FLOAT), f(val) {}
    Variant(const std::string &val) : type(STRING), s(val) {}
    Variant(const char *val) : type(STRING), s(val) {}
    Variant(const Vector3 &val) : type(VECTOR3), v3(val) {}
    Variant(const std::vector<Variant> &val) : type(ARRAY), arr(val) {}
    Variant(const std::unordered_map<std::string, Variant> &val) : type(DICTIONARY), dict(val) {}
};

class RPCHandler
{
public:
    using RPCFunction = std::function<void(uint32_t sender_id,
                                           const std::string &node_path,
                                           const std::string &method,
                                           const std::vector<Variant> &args)>;

    // Callback simplificado para RPCRegistry
    using RPCCallback = std::function<void(uint32_t peer_id, const std::vector<Variant>& args)>;

    RPCHandler() = default;
    ~RPCHandler() = default;

    // ========== Registro de RPCs ==========
    
    // Registra RPC por nome (auto-incrementa ID)
    void registerRPC(const std::string &method, RPCFunction func);
    
    // Registra RPC com ID específico (para sincronizar com Godot)
    void registerRPCWithId(uint16_t id, const std::string &method, RPCFunction func);
    
    // Registra RPC callback simplificado (usado pelo RPCRegistry)
    void registerRPCCallback(const std::string &method, RPCCallback callback);
    void registerRPCCallbackWithId(uint16_t id, const std::string &method, RPCCallback callback);

    // ========== Consultas ==========
    
    // Busca nome do método pelo ID
    std::string getMethodNameById(uint16_t id) const;
    
    // Busca ID do método pelo nome
    uint16_t getMethodIdByName(const std::string& method) const;
    
    // Lista todos os RPCs registrados
    void listRegisteredRPCs() const;

    // ========== Processamento ==========
    
    bool processGodotPacket(uint32_t peer_id, const std::vector<uint8_t> &data);

    std::vector<uint8_t> buildGodotRPCPacket(const std::string &node_path,
                                             const std::string &method,
                                             const std::vector<Variant> &args);

private:
    // Tabelas de mapeamento
    std::unordered_map<std::string, RPCFunction> rpc_table_;         // nome -> callback completo
    std::unordered_map<uint16_t, RPCFunction> rpc_table_by_id_;     // id -> callback completo
    std::unordered_map<std::string, RPCCallback> rpc_callbacks_;     // nome -> callback simples
    std::unordered_map<uint16_t, RPCCallback> rpc_callbacks_by_id_; // id -> callback simples
    std::unordered_map<uint16_t, std::string> method_id_to_name_;   // id -> nome
    std::unordered_map<std::string, uint16_t> method_name_to_id_;   // nome -> id
    
    uint16_t next_method_id_ = 0;

    // decoders
    uint32_t decode_uint32(const uint8_t *p_arr);
    uint16_t decode_uint16(const uint8_t *p_arr);

    // read helpers
    std::string readString(const uint8_t*& ptr, const uint8_t* end);
    uint32_t readU32(const uint8_t*& ptr, const uint8_t* end);
    float readFloat(const uint8_t*& ptr, const uint8_t* end);
    Variant read_variant(const uint8_t*& ptr, const uint8_t* end);
    Variant read_variant_byte_only(const uint8_t*& ptr, const uint8_t* end);

    // write helpers
    void write_u32(std::vector<uint8_t> &buf, uint32_t val);
    void write_u64(std::vector<uint8_t> &buf, uint64_t val);
    void write_string(std::vector<uint8_t> &buf, const std::string &s);
    void write_variant(std::vector<uint8_t> &buf, const Variant &v);
};