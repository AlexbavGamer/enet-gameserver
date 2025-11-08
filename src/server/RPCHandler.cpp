// RPCHandler.cpp
#include "server/RPCHandler.h"
#include "utils/Logger.h"
#include <cstring>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <cmath>

// =============================================================
// Decode helpers
// =============================================================
uint32_t RPCHandler::decode_uint32(const uint8_t *p_arr) {
    uint32_t u = 0;
    for (int i = 0; i < 4; i++) {
        u |= static_cast<uint32_t>(p_arr[i]) << (i * 8);
    }
    return u;
}

uint16_t RPCHandler::decode_uint16(const uint8_t *p_arr) {
    uint16_t u = 0;
    for (int i = 0; i < 2; i++) {
        u |= static_cast<uint16_t>(p_arr[i]) << (i * 8);
    }
    return u;
}

// =============================================================
// Read helpers
// =============================================================
std::string RPCHandler::readString(const uint8_t *&ptr, const uint8_t *end) {
    if (end - ptr < 4)
        throw std::runtime_error("EOF while reading string length");
    uint32_t len = readU32(ptr, end);
    if (len > static_cast<uint32_t>(end - ptr))
        throw std::runtime_error("EOF while reading string data");
    std::string result(reinterpret_cast<const char *>(ptr), len);
    ptr += len;
    return result;
}

uint32_t RPCHandler::readU32(const uint8_t *&ptr, const uint8_t *end) {
    if (end - ptr < 4)
        throw std::runtime_error("EOF while reading u32");
    uint32_t val = decode_uint32(ptr);
    ptr += 4;
    return val;
}

float RPCHandler::readFloat(const uint8_t *&ptr, const uint8_t *end) {
    if (end - ptr < 4)
        throw std::runtime_error("EOF while reading float");
    float v;
    std::memcpy(&v, ptr, 4);
    ptr += 4;
    return v;
}

// =============================================================
// Variant Reader
// =============================================================
Variant RPCHandler::read_variant(const uint8_t*& ptr, const uint8_t* end) {
    if (ptr >= end) throw std::runtime_error("EOF while reading variant type");

    Variant v;
    v.type = static_cast<Variant::Type>(*ptr++);

    switch (v.type) {
        case Variant::BOOL:
            if (ptr >= end) throw std::runtime_error("EOF while reading bool");
            v.b = (*ptr++ != 0);
            break;

        case Variant::INT:
            if (end - ptr < 8) throw std::runtime_error("EOF while reading int64");
            std::memcpy(&v.i, ptr, 8);
            ptr += 8;
            break;

        case Variant::FLOAT:
            if (end - ptr < 8) throw std::runtime_error("EOF while reading double");
            std::memcpy(&v.f, ptr, 8);
            ptr += 8;
            break;

        case Variant::STRING:
            v.s = readString(ptr, end);
            break;

        case Variant::VECTOR3:
            if (end - ptr < 24) throw std::runtime_error("EOF while reading Vector3");
            double x, y, z;
            std::memcpy(&x, ptr, 8); ptr += 8;
            std::memcpy(&y, ptr, 8); ptr += 8;
            std::memcpy(&z, ptr, 8); ptr += 8;
            v.v3 = Vector3(x, y, z);
            break;

        case Variant::ARRAY: {
            uint32_t count = readU32(ptr, end);
            v.arr.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
                v.arr.push_back(read_variant(ptr, end));
            break;
        }

        case Variant::DICTIONARY: {
            uint32_t count = readU32(ptr, end);
            for (uint32_t i = 0; i < count; ++i) {
                std::string key = readString(ptr, end);
                Variant val = read_variant(ptr, end);
                v.dict.emplace(std::move(key), std::move(val));
            }
            break;
        }

        default:
            v.type = Variant::NIL;
            break;
    }

    return v;
}

// Variant para modo byte_only (tipo DEPOIS do valor!)
Variant RPCHandler::read_variant_byte_only(const uint8_t*& ptr, const uint8_t* end) {
    // No modo byte_only, o formato é: [8 bytes double][1 byte tipo]
    if (end - ptr < 9) throw std::runtime_error("EOF while reading byte_only variant");
    
    Variant v;
    
    // Lê 8 bytes como double
    double d;
    std::memcpy(&d, ptr, 8);
    ptr += 8;
    
    // Lê o tipo (1 byte)
    v.type = static_cast<Variant::Type>(*ptr++);
    
    // Converte conforme o tipo
    switch (v.type) {
        case Variant::FLOAT:
            v.f = d;
            break;
        case Variant::INT:
            v.i = static_cast<int64_t>(d);
            break;
        case Variant::BOOL:
            v.b = (d != 0.0);
            break;
        default:
            v.type = Variant::NIL;
            break;
    }
    
    return v;
}

// =============================================================
// Write helpers
// =============================================================
void RPCHandler::write_u32(std::vector<uint8_t> &buf, uint32_t val) {
    uint8_t tmp[4];
    std::memcpy(tmp, &val, 4);
    buf.insert(buf.end(), tmp, tmp + 4);
}

void RPCHandler::write_u64(std::vector<uint8_t> &buf, uint64_t val) {
    uint8_t tmp[8];
    std::memcpy(tmp, &val, 8);
    buf.insert(buf.end(), tmp, tmp + 8);
}

void RPCHandler::write_string(std::vector<uint8_t> &buf, const std::string &s) {
    write_u32(buf, static_cast<uint32_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

void RPCHandler::write_variant(std::vector<uint8_t> &buf, const Variant &v) {
    buf.push_back(static_cast<uint8_t>(v.type));
    switch (v.type) {
        case Variant::BOOL:
            buf.push_back(v.b ? 1 : 0);
            break;
        case Variant::INT:
            write_u64(buf, v.i);
            break;
        case Variant::FLOAT:
            write_u64(buf, *reinterpret_cast<const uint64_t *>(&v.f));
            break;
        case Variant::STRING:
            write_string(buf, v.s);
            break;
        case Variant::VECTOR3:
            write_u64(buf, *reinterpret_cast<const uint64_t *>(&v.v3.x));
            write_u64(buf, *reinterpret_cast<const uint64_t *>(&v.v3.y));
            write_u64(buf, *reinterpret_cast<const uint64_t *>(&v.v3.z));
            break;
        case Variant::ARRAY:
            write_u32(buf, static_cast<uint32_t>(v.arr.size()));
            for (const auto &a : v.arr)
                write_variant(buf, a);
            break;
        case Variant::DICTIONARY:
            write_u32(buf, static_cast<uint32_t>(v.dict.size()));
            for (const auto &[k, val] : v.dict) {
                write_string(buf, k);
                write_variant(buf, val);
            }
            break;
        default:
            break;
    }
}

// =============================================================
// Register RPC
// =============================================================
void RPCHandler::registerRPC(const std::string &method, RPCFunction func) {
    uint16_t id = next_method_id_++;
    rpc_table_[method] = func;
    rpc_table_by_id_[id] = func;
    method_id_to_name_[id] = method;
    method_name_to_id_[method] = id;
    Logger::info("✅ RPC Registered: '" + method + "' -> ID " + std::to_string(id));
}

void RPCHandler::registerRPCWithId(uint16_t id, const std::string &method, RPCFunction func) {
    rpc_table_[method] = func;
    rpc_table_by_id_[id] = func;
    method_id_to_name_[id] = method;
    method_name_to_id_[method] = id;
    if (id >= next_method_id_) next_method_id_ = id + 1;
    Logger::info("✅ RPC Registered: '" + method + "' -> ID " + std::to_string(id));
}

void RPCHandler::registerRPCCallback(const std::string &method, RPCCallback callback) {
    uint16_t id = next_method_id_++;
    RPCFunction wrapper = [callback](uint32_t peer_id, const std::string &, const std::string &, const std::vector<Variant> &args) {
        callback(peer_id, args);
    };
    rpc_table_[method] = wrapper;
    rpc_table_by_id_[id] = wrapper;
    rpc_callbacks_[method] = callback;
    rpc_callbacks_by_id_[id] = callback;
    method_id_to_name_[id] = method;
    method_name_to_id_[method] = id;
    Logger::info("✅ RPC Callback Registered: '" + method + "' -> ID " + std::to_string(id));
}

void RPCHandler::registerRPCCallbackWithId(uint16_t id, const std::string &method, RPCCallback callback) {
    RPCFunction wrapper = [callback](uint32_t peer_id, const std::string &, const std::string &, const std::vector<Variant> &args) {
        callback(peer_id, args);
    };
    rpc_table_[method] = wrapper;
    rpc_table_by_id_[id] = wrapper;
    rpc_callbacks_[method] = callback;
    rpc_callbacks_by_id_[id] = callback;
    method_id_to_name_[id] = method;
    method_name_to_id_[method] = id;
    if (id >= next_method_id_) next_method_id_ = id + 1;
    Logger::info("✅ RPC Callback Registered: '" + method + "' -> ID " + std::to_string(id));
}

// =============================================================
// Queries
// =============================================================
std::string RPCHandler::getMethodNameById(uint16_t id) const {
    auto it = method_id_to_name_.find(id);
    return (it != method_id_to_name_.end()) ? it->second : "";
}

uint16_t RPCHandler::getMethodIdByName(const std::string &method) const {
    auto it = method_name_to_id_.find(method);
    return (it != method_name_to_id_.end()) ? it->second : 0xFFFF;
}

void RPCHandler::listRegisteredRPCs() const {
    Logger::info("\n========== Registered RPCs ==========");
    for (const auto &[id, name] : method_id_to_name_) {
        Logger::info("  ID " + std::to_string(id) + " -> " + name);
    }
    Logger::info("=====================================\n");
}

// =============================================================
// Build Godot RPC Packet
// =============================================================
std::vector<uint8_t> RPCHandler::buildGodotRPCPacket(const std::string &node_path,
                                                     const std::string &method,
                                                     const std::vector<Variant> &args) {
    std::vector<uint8_t> buf;
    buf.push_back(0x20);
    write_string(buf, node_path);
    write_string(buf, method);
    write_u32(buf, static_cast<uint32_t>(args.size()));
    for (const auto &a : args)
        write_variant(buf, a);
    return buf;
}

// =============================================================
// Process Godot Packet
// =============================================================
bool RPCHandler::processGodotPacket(uint32_t peer_id, const std::vector<uint8_t>& data) {
    if (data.size() < 10 || data[0] != 0x20) {
        Logger::error("Invalid RPC packet: size=" + std::to_string(data.size()));
        return false;
    }

    // Skip command byte (0x20)
    const uint8_t* ptr = data.data() + 1;
    const uint8_t* end = data.data() + data.size();

    // Parse meta byte
    uint8_t meta = *ptr++;
    int node_comp = meta & 0x03;
    int name_comp = (meta >> 2) & 0x01;
    bool byte_only = (meta >> 3) & 0x01;

    // Parse node target
    uint32_t node_target = 0;
    if (node_comp == 0) {
        node_target = *ptr++;
    } else if (node_comp == 1) {
        node_target = decode_uint16(ptr); ptr += 2;
    } else if (node_comp == 2 || node_comp == 3) {
        node_target = decode_uint32(ptr); ptr += 4;
    } else {
        Logger::error("Invalid node compression: " + std::to_string(node_comp));
        return false;
    }

    // Parse method ID
    uint16_t method_id = 0;
    if (name_comp == 0) {
        method_id = *ptr++;
    } else if (name_comp == 1) {
        method_id = decode_uint16(ptr); ptr += 2;
    }

    std::string method_name = getMethodNameById(method_id);
    if (method_name.empty()) {
        Logger::warning("RPC not registered: ID " + std::to_string(method_id));
        return false;
    }

    // Parse arguments
    std::vector<Variant> args;

    if (byte_only) {
        // byte_only mode: [3pad][4float][1type][3pad][4float][1type]...
        // Last argument may not have type byte (node path starts)
        if (ptr + 3 <= end) {
            ptr += 3; // Skip initial padding
        }
        
        while (ptr + 4 <= end) {
            // Read float32
            float f;
            std::memcpy(&f, ptr, 4);
            ptr += 4;
            
            // Check for type byte
            Variant v;
            if (ptr < end && *ptr <= 7) {
                // Valid type byte
                v.type = static_cast<Variant::Type>(*ptr++);
                
                switch (v.type) {
                    case Variant::FLOAT: v.f = static_cast<double>(f); break;
                    case Variant::INT: v.i = static_cast<int64_t>(f); break;
                    case Variant::BOOL: v.b = (f != 0.0f); break;
                    default: v.type = Variant::NIL; break;
                }
                
                // Skip padding
                if (ptr + 3 < end && ptr[3] <= 7) {
                    ptr += 3;
                }
            } else {
                // No valid type byte, assume FLOAT
                v.type = Variant::FLOAT;
                v.f = static_cast<double>(f);
            }
            
            args.push_back(v);
            
            // Stop if no more space or found invalid data
            if (ptr >= end || (ptr < end && *ptr > 7 && *ptr < 0x20)) {
                break;
            }
        }
    } else {
        // Normal mode: arg_count + typed variants
        if (ptr >= end) {
            Logger::error("Not enough bytes for arg_count");
            return false;
        }
        
        int arg_count = *ptr++;
        args.reserve(arg_count);

        for (int i = 0; i < arg_count && ptr < end; ++i) {
            try {
                Variant v = read_variant(ptr, end);
                args.push_back(v);
            } catch (const std::exception& e) {
                Logger::error("Error reading argument " + std::to_string(i) + ": " + e.what());
                return false;
            }
        }
    }

    // Log arguments
    for (size_t i = 0; i < args.size(); ++i) {
        std::string s;
        switch (args[i].type) {
            case Variant::FLOAT:  s = "FLOAT: "  + std::to_string(args[i].f); break;
            case Variant::INT:    s = "INT: "    + std::to_string(args[i].i); break;
            case Variant::BOOL:   s = "BOOL: "   + std::string(args[i].b ? "true" : "false"); break;
            case Variant::STRING: s = "STRING: '" + args[i].s + "'"; break;
            case Variant::NIL:    s = "NIL"; break;
            default:              s = "Type " + std::to_string((int)args[i].type); break;
        }
        Logger::info("  Arg[" + std::to_string(i) + "]: " + s);
    }

    // === CALL RPC ===
    auto it = rpc_table_by_id_.find(method_id);
    if (it != rpc_table_by_id_.end()) {
        Logger::info("CALLING RPC: '" + method_name + "' on node " + std::to_string(node_target));
        it->second(peer_id, "node_" + std::to_string(node_target), method_name, args);
        return true;
    }

    Logger::warning("RPC not registered: ID " + std::to_string(method_id) + " ('" + method_name + "')");
    return false;
}