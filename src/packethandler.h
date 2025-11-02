#pragma once
#include <enet/enet.h>
#include <nlohmann/json.hpp>
#include "server_common.h"
using json = nlohmann::json;

// ==============================
// 2. CLASSE PacketHandler
// ==============================
class PacketHandler {
public:
    static json parse(ENetPacket* packet) {
        std::string data((char*)packet->data, packet->dataLength);
        try {
            return json::parse(data);
        } catch (...) {
            return {};
        }
    }

    static ENetPacket* create(PacketID id, const json& data = {}) {
        json packet = { {"id", id}, {"data", data} };
        std::string str = packet.dump();
        return enet_packet_create(str.c_str(), str.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    }
};