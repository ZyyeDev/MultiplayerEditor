#pragma once

#include <iostream>
#include <cstdint>
#include <cstring>
#include <array>
#include <ctime>
#include <Geode/Geode.hpp>

const int maxPlayers = 128;

struct SavedColorData {
    int colorID;
    cocos2d::ccColor3B color;
    bool blending;
    float opacity;
    int copyID;
};

enum class PacketType : uint8_t {
    HANDSHAKE = 0,
    FULL_SYNC = 1,
    OBJECT_ADD = 2,
    OBJECT_DELETE = 3,
    OBJECT_UPDATE = 4,
    MOUSE_MOVE = 5,
    SELECT_CHANGE = 6,
    LEVEL_SETTINGS = 7,
    PLAYER_POSITION = 8,
    PEER_JOINED = 9,
    PEER_LEFT = 10,
    LOBBY_SYNC = 11,
    FULL_SYNC_REQUEST = 12,
    FULL_SYNC_END = 13,
    KICK_USER = 14,
    COLOR_SYNC = 15,
};

#pragma pack(push, 1)

struct PacketHeader{
    PacketType type;
    uint32_t timestamp;
    uint32_t senderID;
};

struct KickPacket {
    PacketHeader header;
    uint32_t userToKick;
    char kickReason[128];
};

struct HandshakePacket{
    PacketHeader header;
    char username[64];
    char version[32];
};

struct ObjectStringPacket {
    PacketHeader header;
    char uid[32];
    uint32_t stringLength;
    char objectString[8192];
};

struct ObjectDeletePacket{
    PacketHeader header;
    char uid[32];
};

struct FullSyncPacket{
    PacketHeader header;
    uint32_t objectCount;
    // TODO: Send complete level, with all objects
};

struct MousePacket{
    PacketHeader header;
    int x;
    int y;
};

struct ColorChannelsPacket{
    PacketHeader header;
    std::vector<SavedColorData> colorDat;
};

struct LevelSettingsPacket {
    PacketHeader header;
    uint32_t settingsLength;
    int audioTrack;
    int songID;
    int levelLength;
    char settingsString[8192];
};

struct FullSyncEndPacket {
    PacketHeader header;
};

struct PlayerIconData{
    int iconID;
    int shipID;
    int ballID;
    int ufoID;
    int waveID;
    int robotID;
    int spiderID;
    int swingID;
    int jetpackID;
    
    int color1ID;
    int color2ID;
    int glowColor;
    
    bool hasGlow;
};

struct PlayerPositionPacket{
    PacketHeader header;
    float x;
    float y;
    float rotation;
    bool isUpsideDown;
    bool isDead;
    bool stopPlaytest;
    PlayerIconData iconData;
};

struct SelectPacket{
    PacketHeader header;
    bool hasMore; // indicates more packets coming (50 objects per select packet)
    uint32_t chunkIndex;
    uint32_t totalCount;
    uint32_t countInChunk;
    char uids[50][32];
};

struct PeerJoinedPacket{
    PacketHeader header;
    uint32_t peerID;
    char username[64];
};

struct PeerLeftPacket{
    PacketHeader header;
    uint32_t peerID;
};

struct FullSyncRequestPacket {
    PacketHeader header;
};

struct LobbyMember{
    uint32_t peerID;
    char username[64];
};

struct LobbySyncPacket{
    PacketHeader header;
    uint32_t memberCount;
    LobbyMember members[maxPlayers];
};

#pragma pack(pop)

inline uint32_t getCurrentTimestamp() {
    return static_cast<uint32_t>(std::time(nullptr));
}