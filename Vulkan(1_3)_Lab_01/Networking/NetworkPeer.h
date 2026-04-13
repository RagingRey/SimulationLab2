#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct SimStatePacket {
    uint32_t objectId = 0;
    uint8_t owner = 0;
    float pos[3]{};
    float vel[3]{};
    uint32_t tick = 0;
};

enum class NetCommandType : uint8_t {
    Play = 1,
    Pause = 2,
    Reset = 3,
    SetTimeStep = 4,
    SetSpeed = 5,
    RequestResync = 6,
    SetScene = 7
};

struct SimCommandPacket {
    NetCommandType command = NetCommandType::Play;
    float value = 0.0f;
    uint32_t tick = 0;
};

// NEW: spawn-distribution packet
struct SimSpawnPacket {
    uint32_t objectId = 0;
    uint8_t owner = 0;
    uint8_t shape = 0; // SimRuntime::SpawnerShapeType cast
    char material[32]{};
    float pos[3]{};
    float vel[3]{};
    float size[3]{};
    float radius = 0.0f;
    float height = 0.0f;
    uint32_t tick = 0;
};

class NetworkPeer {
public:
    NetworkPeer() = default;
    ~NetworkPeer();

    bool Initialize(uint16_t localPort);
    void Shutdown();

    bool SetRemote(const std::string& ip, uint16_t port);

    bool SendState(const SimStatePacket& packet);
    std::vector<SimStatePacket> ReceiveStates();

    bool SendCommand(const SimCommandPacket& packet);
    std::vector<SimCommandPacket> ReceiveCommands();

    // NEW
    bool SendSpawn(const SimSpawnPacket& packet);
    std::vector<SimSpawnPacket> ReceiveSpawns();

    bool IsInitialized() const { return m_Initialized; }
    uint16_t GetLocalPort() const { return m_LocalPort; }

private:
    bool m_Initialized = false;
    bool m_HasRemote = false;
    uint16_t m_LocalPort = 0;

    uintptr_t m_Socket = static_cast<uintptr_t>(-1);
    uint32_t m_RemoteAddr = 0;
    uint16_t m_RemotePort = 0;
};