#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditorUI.hpp>

#include "../sync/SyncManager.hpp"
#include "../network/NetworkManager.hpp"
#include "../network/Packets.hpp"

using namespace geode::prelude;

extern SyncManager* g_sync;
extern NetworkManager* g_network;
extern bool g_isInSession;
extern bool g_isHost;

void objectModified(GameObject* object){
    if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
        g_sync->onLocalObjectModified(object);
    }
}

void settingsUpdate(){
    if (g_isInSession && g_sync && g_isHost && !g_sync->isApplyingRemoteChanges()) {
        g_sync->onLocalLevelSettingsChanged();
    }
}

class $modify(MyLevelEditorLayer, LevelEditorLayer){
    struct Fields {
        bool m_playtesting = false;
    };

    void onEnterTransitionDidFinish() {
        LevelEditorLayer::onEnterTransitionDidFinish();
        if (g_isInSession && !g_isHost && g_network) {
            FullSyncRequestPacket pkt;
            pkt.header.type = PacketType::FULL_SYNC_REQUEST;
            pkt.header.timestamp = getCurrentTimestamp();
            pkt.header.senderID = g_network->getPeerID();
            g_network->sendPacket(&pkt, sizeof(pkt));
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

    /* -- add obj -- */
    void addKeyframe(KeyframeGameObject* p0) {
        // todo
        LevelEditorLayer::addKeyframe(p0);
    }

    void addDelayedSpawn(EffectGameObject* p0, float p1) {
        // todo
        LevelEditorLayer::addDelayedSpawn(p0, p1);
    }

    void addSpecial(GameObject* p0) {
        LevelEditorLayer::addSpecial(p0);

        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
            // HACK: Objects in custom objects tab are detected as objects and are being sent
            // even tho they are part of the ui. We dont want to send these!!
            auto editorUI = this->m_editorUI;
            if (editorUI) {
                CCNode* parent = p0->getParent();
                bool isUIElement = false;
                
                while (parent) {
                    if (parent == editorUI) {
                        isUIElement = true;
                        break;
                    }
                    parent = parent->getParent();
                }
                
                if (!isUIElement) {
                    g_sync->onLocalObjectAdded(p0);
                }
            }
        }
    }

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
};

class $modify(EditorUI){
    bool init(LevelEditorLayer* editorLayer) {
        if (!EditorUI::init(editorLayer)) return false;
        if (g_isInSession && !g_isHost){
            auto settingsbtn = this->getChildByID("settings-button");
            if (settingsbtn){
                settingsbtn->setVisible(false);
            }
        }
        return true;
    }

    /* -- TRANSFORM -- */
    void transformObjectCall(EditCommand cmd) {
        EditorUI::transformObjectCall(cmd);

        auto selected = this->getSelectedObjects();
        for (auto obj : CCArrayExt<GameObject*>(selected)){
            objectModified(obj);
        }
    }

    void scaleObjects(CCArray* p0, float p1, float p2, CCPoint p3, ObjectScaleType p4, bool p5) {
        EditorUI::scaleObjects(p0, p1, p2, p3, p4, p5);
        for (auto obj : CCArrayExt<GameObject*>(p0)){
            objectModified(obj);
        }
    }

    void rotateObjects(CCArray* p0, float p1, CCPoint p2) {
        EditorUI::rotateObjects(p0, p1, p2);
        for (auto obj : CCArrayExt<GameObject*>(p0)){
            objectModified(obj);
        }
    }

    /* -- delete --*/
    void onDeleteSelected(CCObject* sender) {
        if (g_isInSession && g_sync){
            auto selected = this->getSelectedObjects();
            for (auto obj : CCArrayExt<GameObject*>(selected)){
                if (g_sync->isTrackedObject(obj)) {
                    g_sync->onLocalObjectDestroyed(obj);
                }
            }
        }

        EditorUI::onDeleteSelected(sender);
    }

    void onDuplicate(CCObject* sender) {
        EditorUI::onDuplicate(sender);

        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
            auto selected = this->getSelectedObjects();
            for (auto obj : CCArrayExt<GameObject*>(selected)){
                if (!g_sync->isTrackedObject(obj)) {
                    g_sync->onLocalObjectAdded(obj);
                }
            }
        }
    }

    /*
    void pasteObjects(gd::string p0) {
        EditorUI::pasteObjects(p0);

        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
            auto selected = this->getSelectedObjects();
            for (auto obj : CCArrayExt<GameObject*>(selected)){
                if (!g_sync->isTrackedObject(obj)) {
                    g_sync->onLocalObjectAdded(obj);
                }
            }
        }
    }
    */

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

    void ccTouchEnded(CCTouch* touch, CCEvent* event) {
        EditorUI::ccTouchEnded(touch, event);

        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
            auto selected = this->getSelectedObjects();
            for (auto obj : CCArrayExt<GameObject*>(selected)){
                objectModified(obj);
            }
        }
    }

    // possible settings modified
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
        
        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
            g_sync->onLocalSelectionChanged(objects);
        }
    }

    void deselectAll() {
        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
            auto emptyArray = CCArray::create();
            g_sync->onLocalSelectionChanged(emptyArray);
        }
        
        EditorUI::deselectAll();
    }

    /*
    void onUndoButton(CCObject* sender) {
        EditorUI::onUndoButton(sender);

        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
            g_sync->syncAfterUndoRedo();
        }
    }

    void onRedoButton(CCObject* sender) {
        EditorUI::onRedoButton(sender);

        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()){
            g_sync->syncAfterUndoRedo();
        }
    }
    */
};