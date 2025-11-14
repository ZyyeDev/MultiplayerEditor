#pragma once

#include <iostream>
#include <cstdint>
#include <cstring>
#include <array>
#include <ctime>
#include <Geode/Geode.hpp>

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
};

#pragma pack(push, 1)

struct PacketHeader{
    PacketType type;
    uint32_t timestamp;
    uint32_t senderID;
};

struct ObjectData {
    char uid[32];
    
    // Basic properties
    int objectID;
    float x, y;
    float rotation;
    float scaleX, scaleY;
    
    // Layer properties
    ZLayer zLayer;
    int zOrder;
    int editorLayer;
    int editorLayer2;
    
    // Color properties
    GJSpriteColor baseColorID;           // Base color channel
    int detailColorID;         // Detail color channel
    bool dontEnter;            // Don't enter effect
    bool dontFade;             // Don't fade effect
    
    // Group properties
    std::array<int16_t, 10Ui64> *groups;         // Group IDs
    
    // Trigger properties (for triggers)
    float duration;
    int targetGroupID;
    int centerGroupID;
    bool touchTriggered;
    bool spawnTriggered;
    bool multiTrigger;
    
    // Color trigger specific
    float fadeTime;
    int colorID;
    int targetColor;
    float red, green, blue;
    float opacity;
    bool blending;
    int colorIDSecondary;
    
    // Move trigger specific
    float moveX, moveY;
    EasingType easingType;
    float easingRate;
    bool lockToPlayerX;
    bool lockToPlayerY;
    int targetMode;
    
    // Rotate trigger specific
    float degrees;
    int times360;
    bool lockObjectRotation;
    
    // Pulse trigger specific
    int pulseMode;
    int pulseType;
    float fadeIn;
    float hold;
    float fadeOut;
    bool mainOnly;
    bool detailOnly;
    bool exclusiveMode;
    
    // Spawn trigger specific
    float delayTime;
    
    // Follow trigger specific
    float followXMod;
    float followYMod;
    
    // Shake trigger specific
    float shakeStrength;
    float shakeInterval;
    
    // Animate trigger specific
    int animationID;
    
    // Touch trigger specific
    bool holdMode;
    int toggleMode;
    
    // Count trigger specific
    int itemID;
    int targetCount;
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
    
    // Alpha trigger
    float targetOpacity;
    
    // Toggle trigger
    
    // Stop trigger
    
    // Teleport trigger
    float teleportGravity;
    
    // Random trigger
    int groupIDRandom1;
    int groupIDRandom2;
    int chances;
    
    // Advanced Random trigger
    std::array<int16_t, 10> advancedRandomGroups;
    std::array<int16_t, 10> advancedRandomChances;
    
    // Sequence trigger
    int sequenceCount;
    int minInt;
    int maxInt;
    
    // Spawn particle trigger
    int particleID;
    
    // Time trigger
    int timeMode;
    
    // Event trigger
    int eventID;
    
    // Item edit trigger
    int itemEditMode;
    int itemAmount;
    
    // Gradient properties
    bool useGradient;
    int gradientID;
    
    // HSV properties
    bool hasHSV;
    float hue;
    float saturation;
    float brightness;
    bool saturationChecked;
    bool brightnessChecked;
    
    // Portal properties (for portals)
    bool portalChecked;
    
    // Orb properties (for orbs)
    bool multiActivate_orb;
    
    // Pad properties (for pads)
    
    // Text properties (for text objects)
    char textString[256];
    
    // Item counter properties
    
    // Reverse trigger
    bool reverseSync;
    
    // Rotate gameplay trigger
    int rotateMode;
    
    // Song trigger
    int songID;
    
    // SFX trigger
    int sfxID;
    float volume;
    float pitch;
    bool sfxPreload;
    int sfxGroup;
    bool stopSFX;
    
    // Edit SFX trigger
    int sfxEditMode;
    
    // Camera mode trigger
    int cameraMode;
    
    // Camera offset trigger
    float cameraOffsetX;
    float cameraOffsetY;
    
    // Camera rotate trigger
    float cameraRotation;
    int cameraEasing;
    
    // Camera edge trigger
    float cameraEdge;
    int cameraEdgeTarget;
    
    // Static camera trigger
    
    // Camera zoom trigger
    float cameraZoom;
    
    // Speed change properties (for speed changes)
    float speedMod;
    
    // Visibility properties
    bool isVisible;
    bool editorVisibleOnly;
    
    // Link properties
    int linkedGroup;
    
    // High detail
    bool highDetail;
    
    // Custom object properties (for custom objects like items)
    bool isReferenceOnly;
    
    // Scale properties
    bool scaleControlled;
    
    // Unique ID for special objects
    int uniqueID;
    
    // Audio properties
    int audioTrack;
    
    // Area trigger properties
    bool replaceAudioTrack;
    
    // Edit area properties
    int areaID;
    
    // Keyframe properties
    int keyframeID;
    float keyframeTime;
    int keyframeEasing;
    
    // Advanced follow properties
    int followYSpeed;
    int followYDelay;
    int followYOffset;
    float followYMaxSpeed;
    
    // Gradient layer
    int gradientLayer;
    
    // Enter channel properties
    int enterChannel;
    
    // Player color properties (for player color triggers)
    int playerColorID;
    
    // BG/Ground properties
    bool isBG;
    bool isGround;
    bool isMG;
    
    // 2.2 specific properties
    bool dontBoost;
    bool dontBoostX;
    bool dontBoostY;
    int targetPosMode;
    int targetPosID;
    float targetPosXMod;
    float targetPosYMod;
    int targetPosEasing;
    
    // Advanced trigger properties
    bool dynamicMode;
    int comparisonMode;
    
    // Gradient trigger properties
    bool blendingEnabled;
    int gradientMode;
    
    // Shader properties (2.2)
    int shaderID;
    float shaderValue1;
    float shaderValue2;
    float shaderValue3;
    
    // Area properties
    bool isAreaParent;
    bool isAreaChild;
    int areaParentID;
    
    // Persistent properties
    bool isPersistent;
    
    // Random seed
    int randomSeed;
    
    // Collision block properties
    bool isDynamic;
    int blockID;
    
    // Advanced item edit
    float itemEditValue;
    
    // Particle system properties
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
    
    // Platformer mode properties (2.2)
    bool isPlatformer;
    float platformerSpeed;
    
    // Reverse gameplay
    bool reverseEnabled;
    
    // Extra object properties
    bool isFlippedX;
    bool isFlippedY;
    
    // Custom enter effect
    int enterEffect;
    float enterEffectDuration;
    
    // Animation speed
    float animationSpeed;
    bool randomizeAnimationStart;
    
    // Sequence trigger randomization
    bool sequenceRandomize;
    
    // Reset trigger
    bool resetRemap;
    
    // Time warp properties
    float timeWarpSpeed;
    
    // Area tint
    int areaTintID;
    float areaTintOpacity;
    
    // Checkpoint
    bool isCheckpoint;
    
    // Song offset trigger
    float songOffset;
    
    // Gamemode portal properties
    int gamemodePortalType;
    
    // Player items
    bool hasNoEffects;
    
    // Special properties for optimization
    int customObjectID;
    bool isUnique;
    
    // 2.2 platformer properties
    bool stopJump;
    float jumpHeight;
    
    // More trigger properties
    bool triggerOnExit;
    int activateDelay;
    
    // Follow dampening
    float followDamping;
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

struct FullSyncPacket {
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

struct PlayerPositionPacket {
    PacketHeader header;
    float x;
    float y;
    float rotation;
    bool isUpsideDown;
    bool isDead;
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

#pragma pack(pop)

inline uint32_t getCurrentTimestamp() {
    return static_cast<uint32_t>(std::time(nullptr));
}