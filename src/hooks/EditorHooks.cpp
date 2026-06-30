#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/GJEffectManager.hpp>

#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>

#include "../sync/SyncManager.hpp"
#include "../network/NetworkManager.hpp"
#include "../network/Packets.hpp"

using namespace geode::prelude;

extern SyncManager* g_sync;
extern NetworkManager* g_network;
extern bool g_isInSession;
extern bool g_isHost;

LevelEditorLayer* m_editorUI;

void settingsUpdate(){
    if (g_isInSession && g_sync && g_isHost && !g_sync->isApplyingRemoteChanges()) {
        g_sync->onLocalLevelSettingsChanged();
    }
}

void try_add_object(auto obj){
    if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
        // HACK: Objects in custom objects tab are detected as objects and are being sent
        // even tho they are part of the ui. We dont want to send these!!
        if (m_editorUI) {
            CCNode* parent = obj->getParent();
            bool isUIElement = false;
            
            while (parent) {
                if (parent == m_editorUI) {
                    isUIElement = true;
                    break;
                }
                parent = parent->getParent();
            }
            
            if (!isUIElement) {
                try_add_object(obj);
            }
        }
    }
}

class $modify(MyLevelEditorLayer, LevelEditorLayer){
    struct Fields {
        bool m_playtesting = false;

        ~Fields(){
            if (g_isInSession && g_network){
                g_network->disconnect();
            }
        }
    };
    
    void onExit() {
        LevelEditorLayer::onExit();
        
        if (g_isInSession && g_network){
            g_network->disconnect();
        }
    }

    void levelSettingsUpdated() {
        LevelEditorLayer::levelSettingsUpdated();

        if (g_isInSession && g_isHost && g_network) {
            LevelSettingsPacket settings;
            settings.header.type = PacketType::LEVEL_SETTINGS;
            settings.header.timestamp = getCurrentTimestamp();
            settings.header.senderID = g_network->getPeerID();

            settings.settingsLength = 0;
            if (this->m_levelSettings) {
                gd::string gs = this->m_levelSettings->getSaveString();
                std::string saveStr(gs);
                settings.settingsLength = (uint32_t)std::min(saveStr.size(), sizeof(settings.settingsString) - 1);
                strncpy(settings.settingsString, saveStr.c_str(), settings.settingsLength);
                settings.settingsString[settings.settingsLength] = '\0';
            }

            settings.audioTrack = 0;
            settings.songID = 0;
            settings.levelLength = 0;
            if (this->m_level) {
                settings.audioTrack = this->m_level->m_audioTrack;
                settings.songID = this->m_level->m_songID;
                settings.levelLength = this->m_level->m_levelLength;
            }
            g_network->sendPacket(&settings, sizeof(settings));
        }
    }

    /*
    // why are they inlines :sob:
    void undoLastAction() {
        LevelEditorLayer::undoLastAction();
        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()) {
            g_sync->syncAfterUndoRedo();
        }
    }

    void redoLastAction() {
        LevelEditorLayer::redoLastAction();
        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()) {
            g_sync->syncAfterUndoRedo();
        }
    }
        */

    void onPlaytest() {
        log::info("start playtest");
        m_fields->m_playtesting = true;
        
        LevelEditorLayer::onPlaytest();
    }

    void onStopPlaytest() {
        log::info("stop playtest");

        m_fields->m_playtesting = false;
        if (g_sync) {
            g_sync->cleanUpPlayers();
        }
        
        LevelEditorLayer::onStopPlaytest();
    }

    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI)) return false;
        
        m_editorUI = this->m_editorUI;
        
        if (g_isInSession && g_network && !g_isHost){
            g_network->requestFullSync = true;
        }

        return true;
    }
};

class $modify(MyGJBaseGameLayer, GJBaseGameLayer) {
    void addToSection(GameObject* obj) {
        GJBaseGameLayer::addToSection(obj);

        if (!obj || !g_isInSession || !g_sync || g_sync->isApplyingRemoteChanges()) return;

        auto editor = LevelEditorLayer::get();
        if (!editor || static_cast<GJBaseGameLayer*>(editor) != this) return;

        if (g_sync->isTrackedObject(obj)) return;

        try_add_object(obj);
    }
};

class $modify(MyEditorUI, EditorUI) {
    struct Fields {
        std::map<GameObject*, std::string> m_trackedSelections;
        float m_diffTimer = 0.f;
        float m_lockRefreshTimer = 0.f;
    };

    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;

        /*if (g_isInSession && !g_isHost){
            auto settingsbtn = this->getChildByID("settings-button");
            if (settingsbtn){
                settingsbtn->setVisible(false);
            }
        }*/

        schedule(schedule_selector(MyEditorUI::syncTick), 0.1f);

        return true;
    }

    void syncTick(float dt) {
        if (!g_isInSession || !g_sync || g_sync->isApplyingRemoteChanges()) return;

        auto editor = LevelEditorLayer::get();
        if (!editor || !editor->m_objects) return;

        auto& tracked = m_fields->m_trackedSelections;

        std::vector<GameObject*> currentSelection;
        if (m_selectedObject) currentSelection.push_back(m_selectedObject);
        if (m_selectedObjects) {
            for (auto obj : CCArrayExt<GameObject*>(m_selectedObjects)) {
                if (obj) currentSelection.push_back(obj);
            }
        }

        std::vector<GameObject*> toDeselect;
        std::vector<std::string> toLock;

        for (auto obj : currentSelection) {
            if (!editor->m_objects->containsObject(obj)) continue;

            if (!g_sync->isTrackedObject(obj)) {
                g_sync->onLocalObjectAdded(obj);
            }

            uint32_t lockOwner = 0;
            if (g_sync->isObjectLockedByOther(obj, &lockOwner)) {
                toDeselect.push_back(obj);
                continue;
            }

            if (tracked.find(obj) == tracked.end()) {
                gd::string gs = obj->getSaveString(editor);
                tracked[obj] = std::string(gs);
                toLock.push_back(g_sync->getObjectUid(obj));
            }
        }

        for (auto obj : toDeselect) {
            deselectObject(obj);
            if (m_selectedObject == obj) m_selectedObject = nullptr;
            if (m_selectedObjects && m_selectedObjects->containsObject(obj)) {
                m_selectedObjects->removeObject(obj);
            }
        }

        if (!toLock.empty()) {
            g_sync->onLocalSelectionChanged(this->getSelectedObjects());
        }

        m_fields->m_lockRefreshTimer += dt;
        if (m_fields->m_lockRefreshTimer >= 1.0f) {
            m_fields->m_lockRefreshTimer = 0.f;
            if (!tracked.empty()) {
                g_sync->onLocalSelectionChanged(this->getSelectedObjects());
            }
        }

        std::vector<std::string> updates;
        for (auto it = tracked.begin(); it != tracked.end(); ) {
            GameObject* obj = it->first;

            if (!editor->m_objects->containsObject(obj)) {
                it = tracked.erase(it);
                continue;
            }

            bool stillSelected = std::find(currentSelection.begin(), currentSelection.end(), obj) != currentSelection.end()
                && std::find(toDeselect.begin(), toDeselect.end(), obj) == toDeselect.end();

            gd::string gs = obj->getSaveString(editor);
            std::string current = std::string(gs);

            if (!stillSelected) {
                if (current != it->second) {
                    if (obj->m_objectID != 31) {
                        g_sync->onLocalObjectModified(obj);
                    }
                }
                it = tracked.erase(it);
            } else {
                if (current != it->second) {
                    if (obj->m_objectID != 31) {
                        g_sync->onLocalObjectModified(obj);
                    }
                    it->second = current;
                }
                ++it;
            }
        }

        /*if (g_isInSession && g_network && g_sync){
            ColorChannelsPacket colorPacket;

            colorPacket.header.type = PacketType::LEVEL_SETTINGS;
            colorPacket.header.timestamp = getCurrentTimestamp();
            colorPacket.header.senderID = g_network->getPeerID();

            colorPacket.colorDat = g_sync->saveAllColorChannels(g_sync->getEditorLayer());
                
            g_network->sendPacket(&colorPacket, sizeof(colorPacket));
        }*/

        g_sync->updateLocks(dt);
    }

    void moveObject(GameObject* object, CCPoint offset) {
        if (g_isInSession && g_sync && object) {
            if (g_sync->isObjectLockedByOther(object)) return;
        }
        EditorUI::moveObject(object, offset);
    }

    void onDeleteSelected(CCObject* sender) {
        if (g_isInSession && g_sync){
            auto selected = this->getSelectedObjects();
            auto allowed = CCArray::create();
            bool anyLocked = false;
            uint32_t lockedBy = 0;

            for (auto obj : CCArrayExt<GameObject*>(selected)){
                if (g_sync->isObjectLockedByOther(obj, &lockedBy)){
                    anyLocked = true;
                    continue;
                }
                allowed->addObject(obj);
                if (g_sync->isTrackedObject(obj)) {
                    g_sync->onLocalObjectDestroyed(obj);
                }
            }

            if (anyLocked){
                auto peers = g_network->m_peersInLobby;
                auto nameIt = peers.find(lockedBy);
                std::string name = nameIt != peers.end() ? nameIt->second.c_str() : "someone else";
                std::string msg = "Can't delete, " + name + " is editing that";
                Notification::create(msg.c_str(), NotificationIcon::Warning, 1.0f)->show();
            }

            this->selectObjects(allowed, false);
        }

        EditorUI::onDeleteSelected(sender);
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) {
        EditorUI::ccTouchMoved(touch, event);

        if (g_isInSession && g_sync){
            CCPoint glPos = touch->getLocation();

            auto editorLayer = LevelEditorLayer::get();
            if (!editorLayer) return;

            auto objLayer = editorLayer->m_objectLayer;
            if (!objLayer) return;

            CCPoint worldPos = objLayer->convertToNodeSpace(glPos);

            g_sync->onLocalCursorUpdate(worldPos);
        }
    }

    void selectBuildTab(int p0) {
        EditorUI::selectBuildTab(p0);
        
        if (p0 == 4){
            log::info("change to tab {}",p0);
            settingsUpdate();
        }
    }

    void updateSlider() {
        EditorUI::updateSlider();
        settingsUpdate();
    }

    void toggleMode(CCObject* p0) {
        EditorUI::toggleMode(p0);
        settingsUpdate();
    }

    void songStateChanged() {
        EditorUI::songStateChanged();
        settingsUpdate();
    }

    void selectObjects(CCArray* objects, bool idk) {
        EditorUI::selectObjects(objects, idk);
    }

    void deselectAll() {
        auto& tracked = m_fields->m_trackedSelections;
        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges() && !tracked.empty()){
            auto emptyArray = CCArray::create();
            g_sync->onLocalSelectionChanged(emptyArray);
        }
        tracked.clear();
        
        EditorUI::deselectAll();
    }
};
