#include "NetworkPeer.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#pragma comment(lib, "Ws2_32.lib")

NetworkPeer::~NetworkPeer() {
    Shutdown();
}

bool NetworkPeer::Initialize(uint16_t localPort) {
    if (m_Initialized) return true;

    WSADATA wsaData{};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return false;
    }

    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_port = htons(localPort);
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(s, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr)) == SOCKET_ERROR) {
        closesocket(s);
        WSACleanup();
        return false;
    }

    u_long nonBlocking = 1;
    ioctlsocket(s, FIONBIO, &nonBlocking);

    m_Socket = static_cast<uintptr_t>(s);
    m_LocalPort = localPort;
    m_Initialized = true;
    return true;
}

void NetworkPeer::Shutdown() {
    if (m_Initialized) {
        SOCKET s = static_cast<SOCKET>(m_Socket);
        closesocket(s);
        WSACleanup();
    }

    m_Socket = static_cast<uintptr_t>(-1);
    m_Initialized = false;
    m_HasRemote = false;
    m_LocalPort = 0;
    m_RemoteAddr = 0;
    m_RemotePort = 0;
}

bool NetworkPeer::SetRemote(const std::string& ip, uint16_t port) {
    if (!m_Initialized) return false;

    in_addr addr{};
    if (InetPtonA(AF_INET, ip.c_str(), &addr) != 1) {
        return false;
    }

    m_RemoteAddr = addr.s_addr;
    m_RemotePort = port;
    m_HasRemote = true;
    return true;
}

bool NetworkPeer::SendState(const SimStatePacket& packet) {
    if (!m_Initialized || !m_HasRemote) return false;

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(m_RemotePort);
    remote.sin_addr.s_addr = m_RemoteAddr;

    SOCKET s = static_cast<SOCKET>(m_Socket);
    const int sent = sendto(
        s,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(SimStatePacket)),
        0,
        reinterpret_cast<sockaddr*>(&remote),
        sizeof(remote)
    );

    return sent == sizeof(SimStatePacket);
}

std::vector<SimStatePacket> NetworkPeer::ReceiveStates() {
    std::vector<SimStatePacket> out;
    if (!m_Initialized) return out;

    SOCKET s = static_cast<SOCKET>(m_Socket);

    while (true) {
        sockaddr_in from{};
        int fromLen = sizeof(from);

        // Peek first datagram without consuming it
        char peekBuffer[256]{};
        const int peeked = recvfrom(
            s,
            peekBuffer,
            static_cast<int>(sizeof(peekBuffer)),
            MSG_PEEK,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen
        );

        if (peeked == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) break;
            break;
        }

        // Not a state packet -> leave for command receiver
        if (peeked != sizeof(SimStatePacket)) break;

        SimStatePacket p{};
        fromLen = sizeof(from);
        const int received = recvfrom(
            s,
            reinterpret_cast<char*>(&p),
            static_cast<int>(sizeof(SimStatePacket)),
            0,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen
        );

        if (received == sizeof(SimStatePacket)) {
            out.push_back(p);
        }
        else {
            break;
        }
    }

    return out;
}

bool NetworkPeer::SendCommand(const SimCommandPacket& packet) {
    if (!m_Initialized || !m_HasRemote) return false;

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(m_RemotePort);
    remote.sin_addr.s_addr = m_RemoteAddr;

    SOCKET s = static_cast<SOCKET>(m_Socket);
    const int sent = sendto(
        s,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(SimCommandPacket)),
        0,
        reinterpret_cast<sockaddr*>(&remote),
        sizeof(remote)
    );

    return sent == sizeof(SimCommandPacket);
}

std::vector<SimCommandPacket> NetworkPeer::ReceiveCommands() {
    std::vector<SimCommandPacket> out;
    if (!m_Initialized) return out;

    SOCKET s = static_cast<SOCKET>(m_Socket);

    while (true) {
        sockaddr_in from{};
        int fromLen = sizeof(from);

        // Peek first datagram without consuming it
        char peekBuffer[256]{};
        const int peeked = recvfrom(
            s,
            peekBuffer,
            static_cast<int>(sizeof(peekBuffer)),
            MSG_PEEK,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen
        );

        if (peeked == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) break;
            break;
        }

        // Not a command packet -> leave for state receiver
        if (peeked != sizeof(SimCommandPacket)) break;

        SimCommandPacket p{};
        fromLen = sizeof(from);
        const int received = recvfrom(
            s,
            reinterpret_cast<char*>(&p),
            static_cast<int>(sizeof(SimCommandPacket)),
            0,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen
        );

        if (received == sizeof(SimCommandPacket)) {
            out.push_back(p);
        }
        else {
            break;
        }
    }

    return out;
}

bool NetworkPeer::SendSpawn(const SimSpawnPacket& packet) {
    if (!m_Initialized || !m_HasRemote) return false;

    sockaddr_in remote{};
    remote.sin_family = AF_INET;
    remote.sin_port = htons(m_RemotePort);
    remote.sin_addr.s_addr = m_RemoteAddr;

    SOCKET s = static_cast<SOCKET>(m_Socket);
    const int sent = sendto(
        s,
        reinterpret_cast<const char*>(&packet),
        static_cast<int>(sizeof(SimSpawnPacket)),
        0,
        reinterpret_cast<sockaddr*>(&remote),
        sizeof(remote)
    );

    return sent == sizeof(SimSpawnPacket);
}

std::vector<SimSpawnPacket> NetworkPeer::ReceiveSpawns() {
    std::vector<SimSpawnPacket> out;
    if (!m_Initialized) return out;

    SOCKET s = static_cast<SOCKET>(m_Socket);

    while (true) {
        sockaddr_in from{};
        int fromLen = sizeof(from);

        char peekBuffer[256]{};
        const int peeked = recvfrom(
            s,
            peekBuffer,
            static_cast<int>(sizeof(peekBuffer)),
            MSG_PEEK,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen
        );

        if (peeked == SOCKET_ERROR) {
            const int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) break;
            break;
        }

        if (peeked != sizeof(SimSpawnPacket)) break;

        SimSpawnPacket p{};
        fromLen = sizeof(from);
        const int received = recvfrom(
            s,
            reinterpret_cast<char*>(&p),
            static_cast<int>(sizeof(SimSpawnPacket)),
            0,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen
        );

        if (received == sizeof(SimSpawnPacket)) {
            out.push_back(p);
        }
        else {
            break;
        }
    }

    return out;
}