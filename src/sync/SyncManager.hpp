#pragma once

#include <Geode/Geode.hpp>
#include <map>
#include <string>
#include <unordered_set>
#include <algorithm>

#include "../network/Packets.hpp"

using namespace geode::prelude;

struct PendingAction;
struct RemotePlayer {
    PlayerObject* player;
    std::string userId;
};

class SyncManager{
    private:
        /* -- OBJECT TRACKING -- */
        std::map<std::string, GameObject*> m_syncedObjects;
        std::map<GameObject*, std::string> m_objectToUID;

        void trackObject(const std::string& uid, GameObject* obj);
        void untrackObject(const std::string& uid);

        std::string generateUID();

        bool shouldApplyUpdate(uint32_t remoteTimestamp);
        uint32_t m_lastUpdateTimestamp;

        ccColor3B colorForUser(uint32_t userID);

        int m_objectCounter;
        uint32_t m_userID;

        bool m_applyingRemoteChanges = false;

        /* -- LOCAL OBJECT OWNERSHIP (for undo thingy) -- */
        std::unordered_set<GameObject*> m_localObjects;

        /* -- SELECTION -- */
        std::map<uint32_t, CCSprite*> m_remoteCursors;
        std::map<uint32_t, std::vector<CCSprite*>> m_remoteSelectionHighlights;
        std::map<uint32_t, std::map<std::string, float>> m_remoteSelections;
        CCPoint m_CursorPos;
        CCArray* m_Selection;

        /* -- LEVEL SETTINGS -- */
        std::string extractSettingsString();
        void applySettingsString(const char* str, uint32_t len);

        /* -- PLAYER SYNC -- */
        std::map<uint32_t, RemotePlayer> m_remotePlayers;
        float m_lastPlayerSendTime = 0.0f;

    public:
        SyncManager();

        /* --- LOCAL EVENTS --- */
        // object stuff
        void onLocalObjectAdded(GameObject* obj);
        void onLocalObjectDestroyed(GameObject* obj);
        void onLocalObjectModified(GameObject* obj);

        // selection stuff
        void onLocalCursorUpdate(CCPoint position);
        void onLocalSelectionChanged(CCArray* selectedObjects);
        bool isObjectLockedByOther(GameObject* obj, uint32_t* outUserID = nullptr);
        void updateLocks(float dt);

        // level settings
        void onLocalLevelSettingsChanged();

        // player sync
        void updatePlayerSync(float dt, LevelEditorLayer* editorLayer, bool stopPlaytest);
        void sendPlayerPosition(LevelEditorLayer* editorLayer, bool stopPlaytest);

        /* --- REMOTE EVENTS --- */
        // object stuff
        void onRemoteObjectAdded(const ObjectStringPacket& packet);
        void onRemoteObjectDestroyed(const ObjectDeletePacket& packet);
        void onRemoteObjectModified(const ObjectStringPacket& packet);

        // selection stuff
        void onRemoteCursorUpdate(const uint32_t& userID, int x, int y);
        void onRemoteSelectionChanged(const uint32_t& userID);

        // level settings
        void onRemoteLevelSettingsChanged(const LevelSettingsPacket& packet);

        // player sync
        void onRemotePlayerPosition(const PlayerPositionPacket& packet, LevelEditorLayer* editorLayer);

        /* --- FULL SYNC --- */
        void sendFullState(uint32_t targetPeerID);
        void trackExistingObjects();
        void syncAfterUndoRedo();

        /* -- OTHERS -- */
        void handlePacket(const uint8_t* data, size_t size);

        CCPoint getCursorPosition() const { return m_CursorPos; }
        CCArray* getSelection() const { return m_Selection; }

        void setUserID(const uint32_t id) { m_userID = id; }

        bool isApplyingRemoteChanges() const { return m_applyingRemoteChanges; }

        LevelEditorLayer* getEditorLayer();

        bool isTrackedObject(GameObject* obj);
        std::string getObjectUid(GameObject* obj);

        void cleanUpPlayers();
        void clearAllRemoteState();

        uint32_t getUserID() { return SyncManager::m_userID; }
};