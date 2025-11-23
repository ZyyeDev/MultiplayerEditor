#include "Packets.hpp"
#include "NetworkManager.hpp"
#include <Geode/Geode.hpp>
#include <Geode/binding/GJAccountManager.hpp>

using namespace geode::prelude;

NetworkManager::NetworkManager() : m_host(nullptr), m_peer(nullptr), m_isHost(false) {}

NetworkManager::~NetworkManager() {
    disconnect();
}

bool NetworkManager::host(uint16_t port){
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    m_host = enet_host_create(&address, maxPlayers, 2, 0, 0);
    if (m_host == nullptr){
        log::error("Failed to create enet host");
        return false;
    }
    m_port = port;
    m_isHost = true;

    // add ourself (host)
    addPeer(1, getUsername());

    log::info("Hosting on port {}", port);
    return true;
}

bool NetworkManager::stopHosting(){
    if (m_isHost && m_host){
        enet_host_destroy(m_host);
        m_host = nullptr;
        m_isHost = false;
        return true;
    }
    return false;
}

bool NetworkManager::connect(const std::string& ip, uint16_t port){
    m_host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (m_host == nullptr){
        log::error("Failed to create enet client!");
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address,ip.c_str());
    address.port = port;
    
    m_peer = enet_host_connect(m_host, &address, 2, 0);
    if (m_peer == nullptr){
        log::error("couldnt connect to host!");
        enet_host_destroy(m_host);
        m_host = nullptr;
        return false;
    }

    ENetEvent event;
    if (enet_host_service(m_host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT){
        log::info("connected to {}:{}!",ip,port);
        m_port = port;
        m_isHost = false;
        HandshakePacket packet;
        packet.header.type = PacketType::HANDSHAKE;
        packet.header.timestamp = getCurrentTimestamp();
        packet.header.senderID = getPeerID();
        packet.username = getUsername();
        
        sendPacket(&packet, sizeof(packet));
        if (m_onConnect) m_onConnect();
        return true;
    } else {
        //log::error("connection failed, event.type is {}", event.type);
        enet_peer_reset(m_peer);
        enet_host_destroy(m_host);
        m_peer = nullptr;
        m_host = nullptr;
        return false;
    }
}

void NetworkManager::disconnect(){
    if (m_peer){
        enet_peer_disconnect(m_peer, 0);

        ENetEvent event;
        while (enet_host_service(m_host, &event, 3000) > 0){
            if (event.type == ENET_EVENT_TYPE_DISCONNECT){
                break;
            }
        }
        enet_peer_reset(m_peer);
        m_peer = nullptr;
    }

    if (m_host){
        stopHosting();
    }
}

void NetworkManager::sendPacket(const void* data, size_t size){
    if (!m_peer && !m_isHost) return;

    ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
    if (!packet) return;

    if (m_isHost && m_host->connectedPeers > 0){
        // dont send packets if no peers connected
        if (m_host->connectedPeers == 0){
            enet_packet_destroy(packet);
            return;
        }
        enet_host_broadcast(m_host, 0, packet);
    } else {
        if (!m_peer){
            return;
        }
        enet_peer_send(m_peer, 0, packet);
    }
}

void NetworkManager::poll(){
    if (!m_host) return;

    ENetEvent event;
    while (enet_host_service(m_host, &event, 0) > 0){
        switch (event.type){
            case ENET_EVENT_TYPE_CONNECT:
                log::info("peer connected");

                if (m_isHost) {
                    m_connectedPeers[event.peer->connectID] = event.peer;
                } else {
                    m_peer = event.peer;
                }

                if (m_onConnect) m_onConnect();
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                if (m_onReceive){
                    m_onReceive(event.packet->data, event.packet->dataLength);
                }
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT: {
                uint32_t disconnectedPeerID = event.peer->connectID;
                log::info("peer {} disconnected", disconnectedPeerID);

                if (m_isHost){
                    removePeer(disconnectedPeerID);
                    m_connectedPeers.erase(disconnectedPeerID);
                    broadcastPeerLeft(disconnectedPeerID);
                }else{
                    m_peer = nullptr;
                }
                if (m_onDisconnect) m_onDisconnect();
                break;
            }
            default:
                break;
        };
    }
}

void NetworkManager::setOnRecive(std::function<void(const uint8_t*, size_t)> callback){
    m_onReceive = callback;
}

void NetworkManager::setOnConnect(std::function<void()> callback){
    m_onConnect = callback;
}

void NetworkManager::setOnDisconnect(std::function<void()> callback){
    m_onDisconnect = callback;
}

gd::string NetworkManager::getUsername(){
    m_username = GJAccountManager::get()->m_username;
    return m_username;
}

void NetworkManager::addPeer(uint32_t id, const gd::string& username){
    m_peersInLobby[id] = username;
}

void NetworkManager::removePeer(uint32_t id){
    auto it = m_peersInLobby.find(id);
    if (it != m_peersInLobby.end()){
        m_peersInLobby.erase(it);
    }
}

void NetworkManager::broadcastPeerJoined(uint32_t peerID, const gd::string& username){
    PeerJoinedPacket packet;
    packet.header.type = PacketType::PEER_JOINED;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = getPeerID();
    packet.peerID = peerID;
    strncpy(packet.username, username.c_str(), 63);
    packet.username[63] = '\0';
    
    sendPacket(&packet, sizeof(packet));
}


void NetworkManager::broadcastPeerLeft(uint32_t peerID) {
    PeerLeftPacket packet;
    packet.header.type = PacketType::PEER_LEFT;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = getPeerID();
    packet.peerID = peerID;
    
    sendPacket(&packet, sizeof(packet));
}

void NetworkManager::sendLobbyState(uint32_t targetPeerID) {
    LobbySyncPacket packet;
    packet.header.type = PacketType::LOBBY_SYNC;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = getPeerID();
    
    packet.memberCount = m_peersInLobby.size();
    
    size_t i = 0;
    for (const auto& [id, username] : m_peersInLobby) {
        if (i >= 32) break;
        packet.members[i].peerID = id;
        strncpy(packet.members[i].username, username.c_str(), 63);
        packet.members[i].username[63] = '\0';
        i++;
    }
    
    if (targetPeerID != 0) {
        auto peerIt = m_connectedPeers.find(targetPeerID);
        if (peerIt != m_connectedPeers.end()) {
            ENetPacket* enetPacket = enet_packet_create(&packet, sizeof(packet), ENET_PACKET_FLAG_RELIABLE);
            enet_peer_send(peerIt->second, 0, enetPacket);
        } else {
            log::warn("Tried to send lobby state to unknown peer {}", targetPeerID);
        }
    } else {
        sendPacket(&packet, sizeof(packet));
    }
}