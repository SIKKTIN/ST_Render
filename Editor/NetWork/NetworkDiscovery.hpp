#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using SOCKET_TYPE = SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using SOCKET_TYPE = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR (-1)
#endif

namespace ST {

enum class NetRole { None, Host, Client };
enum class NetState { Idle, Waiting, InRoom };

enum class MsgType : uint8_t {
    JOIN          = 1,
    JOIN_ACK      = 2,
    PLAYER_LEAVE  = 3,
    PLAYER_UPDATE = 4,
};

struct RoomInfo {
    std::string name;
    std::string hostIP;
    uint16_t hostPort = 0;
    uint8_t maxPlayers = 4;
    uint8_t playerCount = 0;
    float lastSeen = 0.0f;
};

struct ConnectedClient {
    SOCKET_TYPE socket = INVALID_SOCKET;
    std::string ipAddress;
    uint16_t port = 0;
    std::vector<uint8_t> recvBuf;
};

class NetworkDiscovery {
public:
    NetworkDiscovery();
    ~NetworkDiscovery();

    static NetworkDiscovery& instance();

    bool startHost(const std::string& roomName, uint8_t maxPlayers = 4);
    void stopHost();
    bool isHosting() const { return m_role == NetRole::Host && m_tcpListenSocket != INVALID_SOCKET; }

    bool startClient();
    void stopClient();
    bool isClient() const { return m_role == NetRole::Client; }

    void update(float deltaTime);

    const std::vector<RoomInfo>& getRooms() const { return m_rooms; }
    void clearStaleRooms();

    bool connectToRoom(int listIndex);
    bool isConnecting() const { return m_connecting; }
    void disconnect();

    bool sendToPeer(const void* data, size_t len);
    int recvFromPeer(void* buf, size_t maxLen);
    bool isConnected() const { return !m_clients.empty(); }

    bool acceptPeer();
    bool hasPeer() const { return !m_clients.empty(); }

    const std::vector<ConnectedClient>& getClients() const { return m_clients; }

    NetRole getRole() const { return m_role; }
    NetState getState() const { return m_state; }

    void setOnPeerConnected(std::function<void()> cb) { m_onPeerConnected = std::move(cb); }
    void setOnPeerDisconnected(std::function<void()> cb) { m_onPeerDisconnected = std::move(cb); }
    void setOnMessage(std::function<void(const uint8_t*, size_t)> cb) { m_onMessage = std::move(cb); }
    void setOnRoomInfoUpdated(std::function<void()> cb) { m_onRoomInfoUpdated = std::move(cb); }

    uint8_t getCurrentPlayers() const { return m_currentPlayers; }
    uint8_t getMaxPlayers() const { return m_maxPlayers; }

private:
    void updateHost(float deltaTime);
    void updateClient(float deltaTime);
    void processIncomingPeerMsg();
    static std::string getLocalIPAddress();
    void closeSocket(SOCKET_TYPE& s);
    bool joinMulticastGroup();
    bool setupMulticastSocket(SOCKET_TYPE sock);
    static std::string getMulticastAddr();

    SOCKET_TYPE m_udpSocket = INVALID_SOCKET;
    SOCKET_TYPE m_tcpListenSocket = INVALID_SOCKET;
    SOCKET_TYPE m_peerSocket = INVALID_SOCKET; // used by Client role only
    std::vector<ConnectedClient> m_clients;    // used by Host role only

    std::vector<uint8_t> m_peerRecvBuf;        // Client: raw bytes accumulated across frames
    uint32_t m_peerPayloadLen = 0;              // Client: how many payload bytes the next msg needs

    NetRole m_role = NetRole::None;
    NetState m_state = NetState::Idle;
    bool m_connecting = false;

    std::string m_roomName;
    uint8_t m_maxPlayers = 4;
    uint8_t m_currentPlayers = 1;
    float m_broadcastTimer = 0.0f;

    std::vector<RoomInfo> m_rooms;

    std::function<void()> m_onPeerConnected;
    std::function<void()> m_onPeerDisconnected;
    std::function<void(const uint8_t*, size_t)> m_onMessage;
    std::function<void()> m_onRoomInfoUpdated;

#ifdef _WIN32
    bool m_wsaStarted = false;
#endif
};

}
