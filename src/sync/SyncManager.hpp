#pragma once

#include <Geode/Geode.hpp>
#include <map>
#include <string>
#include <queue>
#include <string>
#include <algorithm>

#include "../network/Packets.hpp"

using namespace geode::prelude;

struct PendingAction;

class SyncManager{
    private:
        /* -- OBJECT TRACKING -- */
        std::map<std::string, GameObject*> m_syncedObjects;
        std::map<GameObject*, std::string> m_objectToUID;

        void trackObject(const std::string& uid, GameObject* obj);
        void untrackObject(const std::string& uid);

        std::string generateUID();

        GameObject* createObjectFromData(const ObjectData& data);
        void applyObjectData(GameObject* obj, const ObjectData& data);
    
        ObjectData extractObjectData(GameObject* obj);

        bool shouldApplyUpdate(uint32_t remoteTimestamp);
        uint32_t m_lastUpdateTimestamp;

        int m_objectCounter;
        std::string m_userID;

        bool m_applyingRemoteChanges = false;

        /* -- selection -- */
        std::map<std::string, CCSprite*> m_remoteCursors;
        CCPoint m_CursorPos;
        CCArray* m_Selection;

        void showRemoteCursor();
        void highlightRemoteSelection();
    public:
        SyncManager();
        ~SyncManager();

        /* --- LOCAL EVENTS --- */
        // object stuff
        void onLocalObjectAdded(GameObject* obj);
        void onLocalObjectDestroyed(GameObject* obj);
        void onLocalObjectModified(GameObject* obj);
        
        // selection stuff
        void onLocalCursorUpdate(CCPoint position);
        void onLocalSelectionChanged(CCArray* selectedObjects);

        /* --- REMOTE EVENTS --- */
        // object stuff
        void onRemoteObjectAdded(const ObjectAddPacket& packet);
        void onRemoteObjectDestroyed(const ObjectDeletePacket& packet);
        void onRemoteObjectModified(const ObjectModifyPacket& packet);

        // selection stuff
        void onRemoteCursorUpdate(const std::string& userID, int x, int y);
        void onRemoteSelectionChanged(const std::vector<std::string>& uids);

        /* --- FULL SYNC --- */
        void sendFullState();
        void reciveFullState(const uint8_t* data, size_t size);

        /* -- OTHERS -- */
        void handlePacket(const uint8_t* data, size_t size);

        CCPoint getCursorPosition() const { return m_CursorPos; }
        CCArray* getSelection() const { return m_Selection; }

        void setUserID(const std::string id) { m_userID = id; }

        bool isApplyingRemoteChanges() const { return m_applyingRemoteChanges; }

        LevelEditorLayer* getEditorLayer();

        bool isTrackedObject(GameObject* obj);
        std::string getObjectUid(GameObject* obj);
};