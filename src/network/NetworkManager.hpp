#pragma once

#include <cstdint>
#include <cstring>
#include <ctime>

#include <string>
#include <functional>
#include "../../libs/enet/include/enet.h"

class NetworkManager{
    public:
        NetworkManager();
        ~NetworkManager();

        uint16_t m_port = 8080;

        bool host(uint16_t port);
        bool stopHosting();
        bool connect(const std::string& ip, uint16_t port);
        void disconnect();

        uint32_t getPeerID() const {
            if (m_peer){
                return m_peer->connectID;
            }
            if (m_isHost){
                return 1;
            }
            return 0;
        }

        void sendPacket(const void* data, size_t size);
        void sendPacketToPeer(uint32_t peerID, const void* data, size_t size);
        void poll();

        void setOnRecive(std::function<void(const uint8_t*, size_t)> callback);
        void setOnConnect(std::function<void(uint32_t)> callback);
        void setOnDisconnect(std::function<void()> callback);

        bool isConnected() const { return m_peer != nullptr; }
        bool isHost() const { return m_isHost; }
        
        std::map<uint32_t, gd::string> m_peersInLobby;
        gd::string getUsername();

        void addPeer(uint32_t id, const gd::string& username);
        void removePeer(uint32_t id);
        void broadcastPeerJoined(uint32_t peerID, const gd::string& username);
        void broadcastPeerLeft(uint32_t peerID);
        void sendLobbyState(uint32_t targetPeerID = 0); // 0 = send to all

        void gotKicked(std::string reason);

        bool m_pendingKick = false;
        std::string m_pendingKickReason;
    private:
        ENetHost* m_host;
        ENetPeer* m_peer;
        
        bool m_isHost;
        gd::string m_username;

        std::function<void(const uint8_t*, size_t)> m_onReceive;
        std::function<void(uint32_t)> m_onConnect;
        std::function<void()> m_onDisconnect;

        std::map<uint32_t, ENetPeer*> m_connectedPeers;
};