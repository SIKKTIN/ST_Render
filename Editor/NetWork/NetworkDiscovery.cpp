#include "NetworkDiscovery.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

namespace {

// Multicast address for room discovery (239.255.x.x is site-local)
constexpr int DISCOVERY_PORT = 9999;
constexpr int TCP_LISTEN_PORT = 8888;
constexpr float BROADCAST_INTERVAL = 1.0f;
constexpr float ROOM_TIMEOUT = 4.0f;
constexpr size_t MAX_PACKET_SIZE = 4096;

// Multicast address for discovery - site-local, won't leak to internet
// 239.255.255.250 is commonly used for SSDP/mDNS-style discovery
constexpr const char* MULTICAST_ADDR = "239.255.255.250";

}

namespace ST {

NetworkDiscovery::NetworkDiscovery() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0) {
        m_wsaStarted = true;
    }
#endif
}

NetworkDiscovery::~NetworkDiscovery() {
    stopHost();
    stopClient();
#ifdef _WIN32
    if (m_wsaStarted) {
        WSACleanup();
    }
#endif
}

NetworkDiscovery& NetworkDiscovery::instance() {
    static NetworkDiscovery inst;
    return inst;
}

std::string NetworkDiscovery::getMulticastAddr() {
    return MULTICAST_ADDR;
}

std::string NetworkDiscovery::getLocalIPAddress() {
#ifdef _WIN32
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR)
        return "127.0.0.1";

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &result) != 0)
        return "127.0.0.1";

    char ipStr[INET_ADDRSTRLEN] = "127.0.0.1";
    for (addrinfo* rp = result; rp; rp = rp->ai_next) {
        auto* addr = reinterpret_cast<sockaddr_in*>(rp->ai_addr);
        if (addr->sin_family == AF_INET && addr->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
            inet_ntop(AF_INET, &addr->sin_addr, ipStr, sizeof(ipStr));
            break;
        }
    }
    freeaddrinfo(result);
    return ipStr;
#else
    char ipStr[INET_ADDRSTRLEN] = "127.0.0.1";
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        const char* kDnsIp = "8.8.8.8";
        uint16_t kDnsPort = 53;
        sockaddr_in servAddr = {};
        servAddr.sin_family = AF_INET;
        servAddr.sin_port = htons(kDnsPort);
        inet_pton(AF_INET, kDnsIp, &servAddr.sin_addr);

        if (connect(sock, reinterpret_cast<sockaddr*>(&servAddr), sizeof(servAddr)) == 0) {
            sockaddr_in localAddr = {};
            socklen_t addrLen = sizeof(localAddr);
            if (getsockname(sock, reinterpret_cast<sockaddr*>(&localAddr), &addrLen) == 0) {
                inet_ntop(AF_INET, &localAddr.sin_addr, ipStr, sizeof(ipStr));
            }
        }
        ::close(sock);
    }
    return ipStr;
#endif
}

void NetworkDiscovery::closeSocket(SOCKET_TYPE& s) {
    if (s == INVALID_SOCKET) return;
#ifdef _WIN32
    closesocket(s);
#else
    ::close(s);
#endif
    s = INVALID_SOCKET;
}

bool NetworkDiscovery::setupMulticastSocket(SOCKET_TYPE sock) {
    // Allow multiple sockets to bind to the same port (SO_REUSEADDR)
    int reuse = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<char*>(&reuse), sizeof(reuse)) == SOCKET_ERROR) {
        return false;
    }

    // Set multicast TTL (time-to-live) - keep it on the local network
    int ttl = 4;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL,
                   reinterpret_cast<char*>(&ttl), sizeof(ttl)) == SOCKET_ERROR) {
        // Non-fatal on some platforms
    }

    // Don't loopback our own multicast packets (host doesn't need to see its own broadcast)
    int loop = 0;
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_LOOP,
               reinterpret_cast<char*>(&loop), sizeof(loop));

    return true;
}

bool NetworkDiscovery::joinMulticastGroup() {
    if (m_udpSocket == INVALID_SOCKET) return false;

    ip_mreq mreq = {};
    if (inet_pton(AF_INET, MULTICAST_ADDR, &mreq.imr_multiaddr.s_addr) <= 0) return false;
    mreq.imr_interface.s_addr = INADDR_ANY; // Join on all interfaces

    if (setsockopt(m_udpSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                   reinterpret_cast<char*>(&mreq), sizeof(mreq)) == SOCKET_ERROR) {
        return false;
    }
    return true;
}

bool NetworkDiscovery::startHost(const std::string& roomName, uint8_t maxPlayers) {
    stopHost();
    stopClient();

    m_roomName = roomName;
    m_maxPlayers = maxPlayers;
    m_currentPlayers = 1;
    m_role = NetRole::Host;
    m_state = NetState::Waiting;
    m_peerSocket = INVALID_SOCKET;

    // --- TCP listen socket ---
    m_tcpListenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_tcpListenSocket == INVALID_SOCKET) return false;

    int reuse = 1;
    setsockopt(m_tcpListenSocket, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<char*>(&reuse), sizeof(reuse));

    sockaddr_in tcpAddr = {};
    tcpAddr.sin_family = AF_INET;
    tcpAddr.sin_addr.s_addr = INADDR_ANY;
    tcpAddr.sin_port = htons(TCP_LISTEN_PORT);

    if (bind(m_tcpListenSocket, reinterpret_cast<sockaddr*>(&tcpAddr), sizeof(tcpAddr)) == SOCKET_ERROR) {
        closeSocket(m_tcpListenSocket);
        return false;
    }
    if (listen(m_tcpListenSocket, 1) == SOCKET_ERROR) {
        closeSocket(m_tcpListenSocket);
        return false;
    }

    // --- UDP multicast socket ---
    m_udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_udpSocket == INVALID_SOCKET) {
        closeSocket(m_tcpListenSocket);
        return false;
    }

    if (!setupMulticastSocket(m_udpSocket)) {
        closeSocket(m_tcpListenSocket);
        closeSocket(m_udpSocket);
        return false;
    }

    // Bind to multicast port on all interfaces
    sockaddr_in mcAddr = {};
    mcAddr.sin_family = AF_INET;
    mcAddr.sin_addr.s_addr = INADDR_ANY;
    mcAddr.sin_port = htons(DISCOVERY_PORT);

    if (bind(m_udpSocket, reinterpret_cast<sockaddr*>(&mcAddr), sizeof(mcAddr)) == SOCKET_ERROR) {
        closeSocket(m_tcpListenSocket);
        closeSocket(m_udpSocket);
        return false;
    }

    // Join the multicast group
    if (!joinMulticastGroup()) {
        std::cerr << "[Host] ERROR: joinMulticastGroup failed" << std::endl;
        closeSocket(m_tcpListenSocket);
        closeSocket(m_udpSocket);
        return false;
    }

#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(m_udpSocket, FIONBIO, &mode);
#else
    int flags = fcntl(m_udpSocket, F_GETFL, 0);
    fcntl(m_udpSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    return true;
}

void NetworkDiscovery::stopHost() {
    for (auto& client : m_clients) {
        closeSocket(client.socket);
    }
    m_clients.clear();
    m_currentPlayers = 1;
    closeSocket(m_tcpListenSocket);
    closeSocket(m_udpSocket);
    if (m_role == NetRole::Host) {
        m_role = NetRole::None;
        m_state = NetState::Idle;
        m_broadcastTimer = 0.0f;
    }
}

bool NetworkDiscovery::startClient() {
    stopHost();
    stopClient();

    m_role = NetRole::Client;
    m_state = NetState::Idle;
    m_peerSocket = INVALID_SOCKET;

    m_udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_udpSocket == INVALID_SOCKET) return false;

    if (!setupMulticastSocket(m_udpSocket)) {
        closeSocket(m_udpSocket);
        return false;
    }

    // Bind to multicast port on all interfaces
    sockaddr_in mcAddr = {};
    mcAddr.sin_family = AF_INET;
    mcAddr.sin_addr.s_addr = INADDR_ANY;
    mcAddr.sin_port = htons(DISCOVERY_PORT);

    if (bind(m_udpSocket, reinterpret_cast<sockaddr*>(&mcAddr), sizeof(mcAddr)) == SOCKET_ERROR) {
        closeSocket(m_udpSocket);
        return false;
    }

    // Join the multicast group
    if (!joinMulticastGroup()) {
        closeSocket(m_udpSocket);
        return false;
    }

#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(m_udpSocket, FIONBIO, &mode);
#else
    int flags = fcntl(m_udpSocket, F_GETFL, 0);
    fcntl(m_udpSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    return true;
}

void NetworkDiscovery::stopClient() {
    disconnect();
    closeSocket(m_udpSocket);
    m_rooms.clear();
    if (m_role == NetRole::Client) {
        m_role = NetRole::None;
        m_state = NetState::Idle;
        return;
    }
}

bool NetworkDiscovery::connectToRoom(int listIndex) {
    if (m_role != NetRole::Client) return false;
    if (listIndex < 0 || listIndex >= (int)m_rooms.size()) return false;
    if (m_connecting || m_peerSocket != INVALID_SOCKET) return false;

    disconnect();

    const RoomInfo& room = m_rooms[listIndex];

    m_peerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_peerSocket == INVALID_SOCKET) return false;

    sockaddr_in peerAddr = {};
    peerAddr.sin_family = AF_INET;
    peerAddr.sin_port = htons(room.hostPort);

    if (inet_pton(AF_INET, room.hostIP.c_str(), &peerAddr.sin_addr) <= 0) {
        closeSocket(m_peerSocket);
        return false;
    }

    // Set non-blocking; connect() will return immediately
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(m_peerSocket, FIONBIO, &mode);
#else
    int flags = fcntl(m_peerSocket, F_GETFL, 0);
    fcntl(m_peerSocket, F_SETFL, flags | O_NONBLOCK);
#endif

    // Initiate connection; don't wait for completion
    int ret = ::connect(m_peerSocket, reinterpret_cast<sockaddr*>(&peerAddr), sizeof(peerAddr));
    if (ret == SOCKET_ERROR) {
#ifdef _WIN32
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK && err != WSAEINPROGRESS) {
#else
        int err = errno;
        if (err != EINPROGRESS) {
#endif
            closeSocket(m_peerSocket);
            return false;
        }
    }

    m_connecting = true;
    m_state = NetState::Waiting;
    return true;
}

void NetworkDiscovery::disconnect() {
    if (m_role == NetRole::Client) {
        if (m_peerSocket != INVALID_SOCKET) {
            if (m_onPeerDisconnected) m_onPeerDisconnected();
        }
        closeSocket(m_peerSocket);
        m_peerSocket = INVALID_SOCKET;
        m_connecting = false;
        m_currentPlayers = 0;
        m_state = NetState::Idle;
        m_peerRecvBuf.clear();
        m_peerPayloadLen = 0;
        if (m_onRoomInfoUpdated) m_onRoomInfoUpdated();
    } else if (m_role == NetRole::Host) {
        for (auto& client : m_clients) {
            closeSocket(client.socket);
        }
        m_clients.clear();
        --m_currentPlayers;
        if (m_currentPlayers < 1) m_currentPlayers = 1;
        if (m_onRoomInfoUpdated) m_onRoomInfoUpdated();
    }
}

bool NetworkDiscovery::acceptPeer() {
    if (m_role != NetRole::Host) return false;
    if ((int)m_clients.size() >= m_maxPlayers) return false;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_tcpListenSocket, &readfds);
    timeval tv = {0, 0};

    int ret = select(static_cast<int>(m_tcpListenSocket) + 1, &readfds, nullptr, nullptr, &tv);
    if (ret <= 0) return false;

    sockaddr_in clientAddr = {};
    socklen_t addrLen = sizeof(clientAddr);
    SOCKET_TYPE clientSock = accept(m_tcpListenSocket,
                                    reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);
    if (clientSock == INVALID_SOCKET) return false;

    // Set non-blocking
#ifdef _WIN32
    unsigned long mode = 1;
    ioctlsocket(clientSock, FIONBIO, &mode);
#else
    int flags = fcntl(clientSock, F_GETFL, 0);
    fcntl(clientSock, F_SETFL, flags | O_NONBLOCK);
#endif

    ConnectedClient client;
    client.socket = clientSock;
    char ipStr[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &clientAddr.sin_addr, ipStr, sizeof(ipStr));
    client.ipAddress = ipStr;
    client.port = ntohs(clientAddr.sin_port);

    m_clients.push_back(client);
    m_state = NetState::InRoom;
    std::cout << "[Host] Client connected from " << client.ipAddress << ":" << client.port
              << " (total: " << m_clients.size() << ")" << std::endl;
    if (m_onPeerConnected) m_onPeerConnected();
    return true;
}

void NetworkDiscovery::update(float deltaTime) {
    if (m_role == NetRole::Host) {
        updateHost(deltaTime);
    } else if (m_role == NetRole::Client) {
        updateClient(deltaTime);
    }
}

void NetworkDiscovery::updateHost(float deltaTime) {
    if (m_state != NetState::Waiting && m_state != NetState::InRoom) return;

    // Accept incoming connections every frame (before processing messages)
    if (m_role == NetRole::Host) {
        acceptPeer();
    }

    // Process incoming TCP messages (JOIN, game data, etc.)
    if (m_state == NetState::InRoom) {
        processIncomingPeerMsg();
    }

    m_broadcastTimer += deltaTime;
    if (m_broadcastTimer >= BROADCAST_INTERVAL) {
        m_broadcastTimer = 0.0f;

        std::string ip = getLocalIPAddress();
        std::string msg = "STROOM|";
        msg += m_roomName;
        msg += "|";
        msg += ip;
        msg += "|";
        msg += std::to_string(TCP_LISTEN_PORT);
        msg += "|";
        msg += std::to_string(m_maxPlayers);
        msg += "|";
        msg += std::to_string(m_currentPlayers);

        sockaddr_in mcAddr = {};
        mcAddr.sin_family = AF_INET;
        mcAddr.sin_port = htons(DISCOVERY_PORT);
        inet_pton(AF_INET, MULTICAST_ADDR, &mcAddr.sin_addr);

        int sent = sendto(m_udpSocket, msg.c_str(), static_cast<int>(msg.size()), 0,
               reinterpret_cast<sockaddr*>(&mcAddr), sizeof(mcAddr));
        std::cout << "[Host] Sent multicast (" << sent << " bytes): " << msg << std::endl;
    }
}

void NetworkDiscovery::processIncomingPeerMsg() {
    if (m_role == NetRole::Client) {
        if (m_peerSocket == INVALID_SOCKET) return;

        // State machine:
        //   m_peerPayloadLen == 0  → reading 4-byte header into m_peerRecvBuf
        //   m_peerPayloadLen >  0  → reading payload into m_peerRecvBuf
        if (m_peerPayloadLen == 0) {
            // Need 4-byte header
            if (m_peerRecvBuf.size() >= 4) {
                // Already have a full header buffered — parse it
                uint32_t payloadLen =
                    static_cast<uint32_t>(m_peerRecvBuf[0]) |
                    (static_cast<uint32_t>(m_peerRecvBuf[1]) << 8) |
                    (static_cast<uint32_t>(m_peerRecvBuf[2]) << 16) |
                    (static_cast<uint32_t>(m_peerRecvBuf[3]) << 24);
                std::cout << "[Client] recv header: " << payloadLen << " bytes" << std::endl;
                if (payloadLen > MAX_PACKET_SIZE || payloadLen == 0) {
                    m_peerRecvBuf.clear();
                    return;
                }
                m_peerPayloadLen = payloadLen;
                m_peerRecvBuf.clear(); // keep only the payload (header consumed)
            } else {
                // Read whatever is available right now (may be 0, 1, 2, 3, or 4 bytes)
                uint8_t tmp[4];
                int r = recv(m_peerSocket, reinterpret_cast<char*>(tmp), 4, 0);
                if (r == 0) { disconnect(); return; }
                if (r == SOCKET_ERROR) {
                    // WSAEWOULDBLOCK — nothing buffered this frame; keep what we already have
                    return;
                }
                for (int i = 0; i < r; ++i) m_peerRecvBuf.push_back(tmp[i]);
                return; // loop back next frame to collect the rest
            }
        }

        // --- Reading payload ---
        size_t needed = m_peerPayloadLen - m_peerRecvBuf.size();
        uint8_t* dst = m_peerRecvBuf.data() + m_peerRecvBuf.size();
        int r = recv(m_peerSocket, reinterpret_cast<char*>(dst), static_cast<int>(needed), 0);
        if (r == SOCKET_ERROR) {
            // WSAEWOULDBLOCK — nothing buffered this frame; keep what we have
            return;
        }
        if (r <= 0) { disconnect(); return; }
        for (int i = 0; i < r; ++i) m_peerRecvBuf.push_back(dst[i]);

        // If we still don't have a full payload, loop back next frame
        if (m_peerRecvBuf.size() < m_peerPayloadLen) return;

        // --- Full message received ---
        std::vector<uint8_t> payload = std::move(m_peerRecvBuf);
        m_peerRecvBuf.clear();
        m_peerPayloadLen = 0;

        uint8_t msgType = payload[0];
        std::cout << "[Client] msg complete, type=" << (int)msgType
                  << " (" << payload.size() << " bytes)" << std::endl;

        if (msgType == static_cast<uint8_t>(MsgType::JOIN_ACK)) {
            if (payload.size() >= 3) {
                m_currentPlayers = payload[1];
                std::cout << "[Client] Room updated: " << (int)payload[1]
                          << "/" << (int)payload[2] << std::endl;
                if (m_onRoomInfoUpdated) m_onRoomInfoUpdated();
            }
        } else if (msgType == static_cast<uint8_t>(MsgType::JOIN)) {
            std::cout << "[Client] Received JOIN (unexpected on client)" << std::endl;
        } else if (msgType == static_cast<uint8_t>(MsgType::PLAYER_UPDATE)) {
            if (payload.size() >= 3) {
                m_currentPlayers = payload[1];
                std::cout << "[Client] Player update: " << (int)payload[1]
                          << "/" << (int)payload[2] << std::endl;
                if (m_onRoomInfoUpdated) m_onRoomInfoUpdated();
            }
        } else {
            std::cout << "[Client] Unknown msg type: " << (int)msgType << std::endl;
            if (m_onMessage && !payload.empty()) {
                m_onMessage(payload.data(), payload.size());
            }
        }
        return;
    }

    if (m_role == NetRole::Host) {
        // Iterate in reverse to safely erase disconnected clients
        for (int i = (int)m_clients.size() - 1; i >= 0; --i) {
            ConnectedClient& client = m_clients[i];

            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(client.socket, &readfds);
            timeval tv = { 0, 0 };
            if (select(static_cast<int>(client.socket) + 1, &readfds, nullptr, nullptr, &tv) <= 0) continue;

            uint8_t header[4];
            int received = recv(client.socket, reinterpret_cast<char*>(header), 4, 0);
            if (received == 0) {
                std::cout << "[Host] Client disconnected: " << client.ipAddress << std::endl;
                closeSocket(client.socket);
                m_clients.erase(m_clients.begin() + i);
                --m_currentPlayers;
                if (m_currentPlayers < 1) m_currentPlayers = 1;
                if (m_onRoomInfoUpdated) m_onRoomInfoUpdated();
                if (m_onPeerDisconnected) m_onPeerDisconnected();
                continue;
            }
            if (received < 4) continue;

            uint32_t payloadLen =
                static_cast<uint32_t>(header[0]) |
                (static_cast<uint32_t>(header[1]) << 8) |
                (static_cast<uint32_t>(header[2]) << 16) |
                (static_cast<uint32_t>(header[3]) << 24);

            if (payloadLen > MAX_PACKET_SIZE || payloadLen == 0) continue;

            client.recvBuf.resize(payloadLen);
            size_t totalReceived = 0;
            while (totalReceived < payloadLen) {
                int r = recv(client.socket,
                             reinterpret_cast<char*>(client.recvBuf.data()) + totalReceived,
                             static_cast<int>(payloadLen - totalReceived), 0);
                if (r <= 0) {
                    client.recvBuf.clear();
                    break;
                }
                totalReceived += r;
            }
            if (client.recvBuf.empty()) continue;

            MsgType type = static_cast<MsgType>(client.recvBuf[0]);
            std::cout << "[Host] Client msg type=" << (int)type << " from " << client.ipAddress << std::endl;
            if (type == MsgType::JOIN) {
                ++m_currentPlayers;
                std::cout << "[Host] Client joined (" << client.ipAddress << "), players: "
                          << (int)m_currentPlayers << "/" << (int)m_maxPlayers << std::endl;
                if (m_onRoomInfoUpdated) m_onRoomInfoUpdated();

                // Send JOIN_ACK to the new client
                uint8_t resp[3] = { static_cast<uint8_t>(MsgType::JOIN_ACK), m_currentPlayers, m_maxPlayers };
                uint8_t lenBytes[4] = { 3, 0, 0, 0 };
                send(client.socket, reinterpret_cast<const char*>(lenBytes), 4, 0);
                send(client.socket, reinterpret_cast<const char*>(resp), 3, 0);

                // Broadcast updated player count to all existing clients
                uint8_t update[3] = { static_cast<uint8_t>(MsgType::PLAYER_UPDATE), m_currentPlayers, m_maxPlayers };
                uint8_t updLen[4] = { 3, 0, 0, 0 };
                for (const auto& other : m_clients) {
                    if (other.socket == client.socket) continue;
                    send(other.socket, reinterpret_cast<const char*>(updLen), 4, 0);
                    send(other.socket, reinterpret_cast<const char*>(update), 3, 0);
                }
            } else if (type == MsgType::PLAYER_LEAVE) {
                std::cout << "[Host] Client left (" << client.ipAddress << ")" << std::endl;
                closeSocket(client.socket);
                m_clients.erase(m_clients.begin() + i);
                --m_currentPlayers;
                if (m_currentPlayers < 1) m_currentPlayers = 1;
                if (m_onRoomInfoUpdated) m_onRoomInfoUpdated();

                // Broadcast updated player count to all remaining clients
                uint8_t update[3] = { static_cast<uint8_t>(MsgType::PLAYER_UPDATE), m_currentPlayers, m_maxPlayers };
                uint8_t updLen[4] = { 3, 0, 0, 0 };
                for (const auto& other : m_clients) {
                    send(other.socket, reinterpret_cast<const char*>(updLen), 4, 0);
                    send(other.socket, reinterpret_cast<const char*>(update), 3, 0);
                }
            } else if (m_onMessage && !client.recvBuf.empty()) {
                m_onMessage(client.recvBuf.data(), client.recvBuf.size());
            }
        }
    }
}

void NetworkDiscovery::updateClient(float deltaTime) {
    if (m_udpSocket == INVALID_SOCKET) return;

    // Check if an async connect() has completed
    if (m_connecting && m_peerSocket != INVALID_SOCKET) {
        fd_set writefds;
        FD_ZERO(&writefds);
        FD_SET(m_peerSocket, &writefds);
        timeval tv = { 0, 0 };
        int ret = select(static_cast<int>(m_peerSocket) + 1, nullptr, &writefds, nullptr, &tv);
        if (ret > 0) {
            int sockErr = 0;
            socklen_t errLen = sizeof(sockErr);
            getsockopt(m_peerSocket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&sockErr), &errLen);
                if (sockErr == 0) {
                    // Connection succeeded
                    m_connecting = false;
                    m_state = NetState::InRoom;
                    std::cout << "[Client] Connected to host, sending JOIN..." << std::endl;
                    uint8_t joinMsg[1] = { static_cast<uint8_t>(MsgType::JOIN) };
                    sendToPeer(joinMsg, sizeof(joinMsg));

                    if (m_onPeerConnected) m_onPeerConnected();
            } else {
                // Connection failed
                std::cerr << "[Client] Connection failed (SO_ERROR=" << sockErr << ")" << std::endl;
                closeSocket(m_peerSocket);
                m_connecting = false;
            }
        }
        // If ret == 0, still connecting; do nothing and check again next frame
    }

    // Process incoming TCP messages every frame (JOIN_ACK, game data, etc.)
    if (m_state == NetState::InRoom) {
        processIncomingPeerMsg();
    }

    char buf[MAX_PACKET_SIZE];
    sockaddr_in fromAddr = {};
    socklen_t addrLen = sizeof(fromAddr);

    int n = recvfrom(m_udpSocket, buf, sizeof(buf) - 1, 0,
                     reinterpret_cast<sockaddr*>(&fromAddr), &addrLen);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[Client] Received multicast (" << n << " bytes): " << buf << std::endl;

        std::string_view sv(buf, n);
        constexpr std::string_view prefix = "STROOM|";
        if (sv.rfind(prefix, 0) == 0) {
            sv.remove_prefix(prefix.size());
            std::vector<std::string_view> parts;
            size_t start = 0;
            for (int i = 0; i < 6; ++i) {
                size_t pos = sv.find('|', start);
                if (pos == std::string_view::npos) {
                    if (start < sv.size())
                        parts.push_back(sv.substr(start));
                    break;
                }
                parts.push_back(sv.substr(start, pos - start));
                start = pos + 1;
            }

            if (parts.size() >= 4) {
                RoomInfo room;
                room.name = std::string(parts[0]);
                room.hostIP = std::string(parts[1]);
                room.hostPort = static_cast<uint16_t>(std::stoi(std::string(parts[2])));
                if (parts.size() >= 5) room.maxPlayers = static_cast<uint8_t>(std::stoi(std::string(parts[3])));
                if (parts.size() >= 6) room.playerCount = static_cast<uint8_t>(std::stoi(std::string(parts[4])));
                room.lastSeen = 0.0f;

                bool found = false;
                for (auto& existingRoom : m_rooms) {
                    if (existingRoom.hostIP == room.hostIP && existingRoom.hostPort == room.hostPort) {
                        existingRoom = room;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    m_rooms.push_back(room);
                }
            }
        }
    }

    // Process incoming TCP messages (JOIN_ACK, game data, etc.)
    if (m_state == NetState::InRoom || m_connecting) {
        processIncomingPeerMsg();
    }

    // Age rooms
    for (auto& room : m_rooms) {
        room.lastSeen += deltaTime;
    }
    m_rooms.erase(
        std::remove_if(m_rooms.begin(), m_rooms.end(),
            [](const RoomInfo& info) { return info.lastSeen > ROOM_TIMEOUT; }),
        m_rooms.end()
    );
}

void NetworkDiscovery::clearStaleRooms() {
    m_rooms.clear();
}

bool NetworkDiscovery::sendToPeer(const void* data, size_t len) {
    if (len > MAX_PACKET_SIZE) return false;

    uint32_t netLen = static_cast<uint32_t>(len);
    uint8_t header[4];
    header[0] = static_cast<uint8_t>((netLen >> 0) & 0xFF);
    header[1] = static_cast<uint8_t>((netLen >> 8) & 0xFF);
    header[2] = static_cast<uint8_t>((netLen >> 16) & 0xFF);
    header[3] = static_cast<uint8_t>((netLen >> 24) & 0xFF);

    if (m_role == NetRole::Host) {
        // Broadcast to all connected clients
        for (const auto& client : m_clients) {
            if (client.socket == INVALID_SOCKET) continue;
            send(client.socket, reinterpret_cast<const char*>(header), 4, 0);
            send(client.socket, reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
        }
        return !m_clients.empty();
    }

    if (m_peerSocket == INVALID_SOCKET) return false;
    int sent = send(m_peerSocket, reinterpret_cast<const char*>(header), 4, 0);
    if (sent != 4) return false;
    sent = send(m_peerSocket, reinterpret_cast<const char*>(data), static_cast<int>(len), 0);
    std::cout << "[Client] sendToPeer: sent " << sent << "/" << len << " bytes" << std::endl;
    return sent == static_cast<int>(len);
}

int NetworkDiscovery::recvFromPeer(void* buf, size_t maxLen) {
    if (m_peerSocket == INVALID_SOCKET) return -1;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(m_peerSocket, &readfds);
    timeval tv = { 0, 0 };
    if (select(static_cast<int>(m_peerSocket) + 1, &readfds, nullptr, nullptr, &tv) <= 0) return -1;

    uint8_t header[4];
    int received = recv(m_peerSocket, reinterpret_cast<char*>(header), 4, 0);
    if (received == 0) return -1;
    if (received < 4) return -1;

    uint32_t payloadLen =
        static_cast<uint32_t>(header[0]) |
        (static_cast<uint32_t>(header[1]) << 8) |
        (static_cast<uint32_t>(header[2]) << 16) |
        (static_cast<uint32_t>(header[3]) << 24);

    if (payloadLen > maxLen || payloadLen > MAX_PACKET_SIZE) return -1;

    size_t totalReceived = 0;
    uint8_t* out = static_cast<uint8_t*>(buf);
    while (totalReceived < payloadLen) {
        int r = recv(m_peerSocket,
                     reinterpret_cast<char*>(out + totalReceived),
                     static_cast<int>(payloadLen - totalReceived), 0);
        if (r <= 0) return -1;
        totalReceived += r;
    }

    return static_cast<int>(payloadLen);
}

}
