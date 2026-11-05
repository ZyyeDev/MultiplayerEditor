#pragma once

#include <Geode/Geode.hpp>
#include <map>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

#include "../network/Packets.hpp"

using namespace geode::prelude;

struct RemotePlayer {
    PlayerObject* player = nullptr;
    PlayerObject* player2 = nullptr;
    uint8_t appliedGameMode = 255;
    int appliedIconFrame = -1;
    uint8_t appliedGameMode2 = 255;
    int appliedIconFrame2 = -1;
    std::string appliedRobotAnim;
    std::string appliedRobotAnim2;
};

class SyncManager{
    private:
        std::map<std::string, GameObject*> m_syncedObjects;
        std::map<GameObject*, std::string> m_objectToUID;

        void trackObject(const std::string& uid, GameObject* obj);
        void untrackObject(const std::string& uid);

        GameObject* createAndDiffObjectFromString(LevelEditorLayer* editor, const std::string& objString);

        std::string generateUID();

        ccColor3B colorForUser(uint32_t userID);

        int m_objectCounter;
        uint32_t m_userID;

        bool m_applyingRemoteChanges = false;
        struct ChunkBuffer {
            std::string data;
        };
        std::map<std::string, ChunkBuffer> m_incomingChunks;
        std::map<std::string, std::string> m_pendingObjectUpdates;
        void flushPendingObjectUpdates();

        void sendObjectPackets(PacketType type, const std::string& uid, const std::string& objString);
        std::unordered_set<GameObject*> m_localObjects;
        std::map<uint32_t, CCSprite*> m_remoteCursors;
        std::map<uint32_t, std::vector<CCSprite*>> m_remoteSelectionHighlights;
        std::map<uint32_t, std::map<std::string, float>> m_remoteSelections;
        CCPoint m_CursorPos;

        void removeRemoteCursor(uint32_t userID);
        void onPeerLeft(uint32_t peerID);
        std::string extractSettingsString();
        void applyLevelSettings(LevelSettingsPacket const& settings);
        bool isPopupBlockingLevelSettings();
        bool m_hasPendingLevelSettings = false;
        LevelSettingsPacket m_pendingLevelSettings;
        std::map<uint32_t, RemotePlayer> m_remotePlayers;
        float m_lastPlayerSendTime = 0.0f;
        std::map<int, SavedColorData> m_lastColors;
        std::map<int, SavedColorData> m_pendingColors;
        bool m_colorsSeeded = false;

        std::vector<GJEffectManager*> getColorManagers();
        bool isSyncableColorChannel(int id);
        bool readColor(GJEffectManager* manager, int id, SavedColorData& out);
        void collectColors(std::vector<SavedColorData>& out);
        void sendColors(const std::vector<SavedColorData>& colors, uint32_t targetPeerID);
        void applyColors(const std::vector<SavedColorData>& colors);
        void flushPendingColors();

    public:
        SyncManager();
        void onLocalObjectAdded(GameObject* obj);
        void onLocalObjectDestroyed(GameObject* obj);
        void onLocalObjectModified(GameObject* obj);
        void onLocalCursorUpdate(CCPoint position);
        void onLocalSelectionChanged(CCArray* selectedObjects);
        bool isObjectLockedByOther(GameObject* obj, uint32_t* outUserID = nullptr);
        void updateLocks(float dt);
        void onLocalLevelSettingsChanged();
        void updatePlayerSync(float dt, LevelEditorLayer* editorLayer, bool stopPlaytest);
        void sendPlayerPosition(LevelEditorLayer* editorLayer, bool stopPlaytest);
        void onRemoteObjectAdded(const std::string& uid, const std::string& objString);
        void onRemoteObjectDestroyed(const ObjectDeletePacket& packet);
        void onRemoteObjectModified(const std::string& uid, const std::string& objString);
        void onRemoteCursorUpdate(const uint32_t& userID, int x, int y);
        void onRemoteSelectionChanged(const uint32_t& userID);
        void onRemoteLevelSettingsChanged(const LevelSettingsPacket& packet);
        void onRemotePlayerPosition(const PlayerPositionPacket& packet, LevelEditorLayer* editorLayer);
        void sendFullState(uint32_t targetPeerID);
        void trackExistingObjects();
        void syncAfterUndoRedo();
        void pruneStaleTrackedObjects();
        void handlePacket(const uint8_t* data, size_t size);

        void setUserID(const uint32_t id) { m_userID = id; }

        bool isApplyingRemoteChanges() const { return m_applyingRemoteChanges; }

        LevelEditorLayer* getEditorLayer();

        bool isTrackedObject(GameObject* obj);
        std::string getObjectUid(GameObject* obj);

        void cleanUpPlayers();
        void clearAllRemoteState();
        void clearPeerState(uint32_t peerID);

        void processPendingLevelSettings();
        void updateColorSync();
        void sendAllColors(uint32_t targetPeerID);
        void onRemoteColorsChanged(const ColorChannelsPacket& packet, size_t size);
};