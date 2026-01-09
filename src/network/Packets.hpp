#pragma once

#include <iostream>
#include <cstdint>
#include <cstring>
#include <array>
#include <ctime>
#include <Geode/Geode.hpp>

const int maxPlayers = 128;

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
};

#pragma pack(push, 1)

struct PacketHeader{
    PacketType type;
    uint32_t timestamp;
    uint32_t senderID;
};

struct HandshakePacket{
    PacketHeader header;
    gd::string username;
};

struct ObjectData {
    char uid[32];
    
    // Basic properties
    int objectID;
    float x, y;
    float rotation;
    float scaleX, scaleY;
    bool isFlippedX;
    bool isFlippedY;
    
    // Object state
    bool hasBeenActivated;
    GameObjectType objectType;
    cocos2d::CCPoint startPositionPoint;
    bool useAudioScale;
    
    // Layer properties
    ZLayer zLayer;
    int zOrder;
    int editorLayer;
    int editorLayer2;
    
    // Color properties
    GJSpriteColor baseColorID;
    int detailColorID;
    bool dontEnter;
    bool dontFade;
    
    // HSV properties
    bool baseUsesHSV;
    bool detailUsesHSV;
    bool hasHSV;
    float hue;
    float saturation;
    float brightness;
    bool saturationChecked;
    bool brightnessChecked;
    
    // Group properties
    std::array<int16_t, 10> groups;
    int groupCount;
    
    // Visibility & Detail
    bool isVisible;
    bool highDetail;
    bool editorEnabled;
    
    // Link and unique
    int linkedGroup;
    int uniqueID;
    
    // Text objects
    char textString[256];
    
    // Trigger properties (common to all triggers)
    float duration;
    int targetGroupID;
    int centerGroupID;
    bool touchTriggered;
    bool spawnTriggered;
    bool multiTrigger;
    bool triggerOnExit;
    
    // Touch trigger
    bool touchHoldMode;
    TouchTriggerType touchToggleMode;
    TouchTriggerControl touchPlayerMode;
    
    // Color trigger
    float opacity;
    bool copyOpacity;
    int targetColor;
    int colorID;
    int copyColorID;
    float fadeTime;
    bool blending;
    int colorIDSecondary;
    
    // HSV trigger
    cocos2d::ccHSVValue hsvValue;
    
    // Move trigger
    float moveX, moveY;
    EasingType easingType;
    float easingRate;
    bool lockToPlayerX;
    bool lockToPlayerY;
    bool useMoveTarget;
    MoveTargetType moveTargetMode;
    bool lockToCameraX;
    bool lockToCameraY;
    
    bool dontBoostX;
    bool dontBoostY;
    
    // Rotate trigger
    float degrees;
    int times360;
    bool lockObjectRotation;
    
    // Pulse trigger
    int pulseMode;
    int pulseType;
    float fadeIn;
    float hold;
    float fadeOut;
    bool mainOnly;
    bool detailOnly;
    bool exclusiveMode;
    
    // Spawn trigger
    float spawnDelay;
    
    // Follow trigger
    float followXMod;
    float followYMod;
    int followYSpeed;
    int followYDelay;
    int followYOffset;
    float followYMaxSpeed;
    
    // Shake trigger
    float shakeStrength;
    float shakeInterval;
    
    // Animate trigger
    int animationID;
    float animationSpeed;
    bool randomizeAnimationStart;
    
    // Count trigger
    int itemID;
    int itemID2;
    bool activateGroup;
    bool multiActivate;
    
    // Instant count trigger
    int comparison;
    
    // Pickup trigger
    int pickupMode;
    
    // Collision trigger
    bool activateOnExit;
    int blockAID;
    int blockBID;
    bool isDynamic;
    int blockID;
    
    // Teleport trigger
    float teleportGravity;

    float randomFrameTime;
    
    // Advanced Random trigger
    std::array<int16_t, 10> advancedRandomGroups;
    std::array<int16_t, 10> advancedRandomChances;
    
    // Spawn particle trigger
    int particleID;
    
    // Reverse trigger
    bool isReverse;
    bool reverseEnabled;
    
    // SFX trigger
    /*
    int sfxSound;
    float volume;
    float pitch;
    bool sfxPreload;
    */

    // Camera triggers
    bool isFreeMode;
    float cameraEasing;
    float cameraZoom;
    
    // Speed
    short speedModType;
    
    // Area trigger
    bool hasAreaParent;
    int areaParentID;
    //int areaTintID;
    float areaTintOpacity;
    
    // Enter channel
    int enterChannel;
    short enterType;
    
    // Player color
    int usesPlayerColor1;
    int usesPlayerColor2;
    
    // Advanced trigger properties
    bool dynamicMode;
    int comparisonMode;
    
    // Particle system properties
    bool hasParticles;
    float particleLifetime;
    float particleStartSize;
    float particleEndSize;
    float particleStartSpin;
    float particleEndSpin;
    float particleEmissionRate;
    float particleAngle;
    float particleSpeed;
    float particlePosVarX;
    float particlePosVarY;
    float particleGravityX;
    float particleGravityY;
    float particleAccelRadial;
    float particleAccelTangential;
    int particleStartColorR;
    int particleStartColorG;
    int particleStartColorB;
    int particleStartColorA;
    int particleEndColorR;
    int particleEndColorG;
    int particleEndColorB;
    int particleEndColorA;
    bool particleFadeInTime;
    bool particleFadeOutTime;
    bool particleStartSizeVar;
    bool particleEndSizeVar;
    bool particleStartSpinVar;
    bool particleEndSpinVar;
    
    // Player items
    bool hasNoEffects;

    // Gravity
    float gravityValue;
};

struct ObjectStringPacket {
    PacketHeader header;
    char uid[32];
    uint32_t stringLength;
    char objectString[4096];
};

struct ObjectAddPacket{
    PacketHeader header;
    ObjectData object;
};

struct ObjectDeletePacket{
    PacketHeader header;
    char uid[32];
};

// this should handle ALL changes of objects, including properties, groups, etc.
struct ObjectModifyPacket{
    PacketHeader header;
    char uid[32];
    ObjectData object;
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

struct LevelSettingsData{
    // Audio
    int songID;
    int customSongID;
    float startOffset;
    
    // Colors
    int backgroundColorID;
    int groundColorID;
    int lineColorID;
    int objectColorID;
    int color1ID;
    int color2ID;
    int color3ID;
    int color4ID;
    
    // Gameplay
    int gamemode;
    int miniMode;
    Speed speed;
    int dualMode;
    int twoPlayerMode;
    bool isPlatformer;
    
    // Background/Ground
    int backgroundIndex;
    int groundIndex;
    int fontIndex;
    
    // Guidelines
    float guidelineSpacing;
};

struct LevelSettingsPacket{
    PacketHeader header;
    LevelSettingsData settings;
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