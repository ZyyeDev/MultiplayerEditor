#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditorUI.hpp>

#include "../sync/SyncManager.hpp"

using namespace geode::prelude;

extern SyncManager* g_sync;
extern bool g_isInSession;
extern bool g_isHost;

void objectModified(GameObject* object){
    if (g_isInSession){
        g_sync->onLocalObjectModified(object);
    }
}

class $modify(LevelEditorLayer){
    /* -- add obj -- */
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
};

class $modify(EditorUI){
    /* -- TRANSFORM -- */
    void transformObjectCall(EditCommand cmd) {
        EditorUI::transformObjectCall(cmd);

        auto selected = this->getSelectedObjects();
        for (auto obj : CCArrayExt<GameObject*>(selected)){
            objectModified(obj);
        }
    }

    void scaleObjects(CCArray* p0, float p1, float p2, CCPoint p3, ObjectScaleType p4, bool p5) {
        for (auto obj : CCArrayExt<GameObject*>(p0)){
            objectModified(obj);
        }
        EditorUI::scaleObjects(p0, p1, p2, p3, p4, p5);
    }

    void rotateObjects(CCArray* p0, float p1, CCPoint p2) {
        for (auto obj : CCArrayExt<GameObject*>(p0)){
            objectModified(obj);
        }
        EditorUI::rotateObjects(p0, p1, p2);
    }

    // idk if this is really related to transform things, but ill leave it just in case
    void updateButtons() {
        if (g_isInSession && g_sync && !g_sync->isApplyingRemoteChanges()) {
            auto selected = this->getSelectedObjects();
            if (selected && selected->count() > 0) {
                for (auto obj : CCArrayExt<GameObject*>(selected)) {
                    g_sync->onLocalObjectModified(obj);
                }
            }
        }

        EditorUI::updateButtons();
    }
    /* -- delete --*/
    void onDeleteSelected(CCObject* sender) {
        auto selected = this->getSelectedObjects();

        if (g_isInSession && g_sync){
            for (auto obj : CCArrayExt<GameObject*>(selected)){
                g_sync->onLocalObjectDestroyed(obj);
            }
        }

        EditorUI::onDeleteSelected(sender);
    }

    void ccTouchMoved(CCTouch* touch, CCEvent* event) {
        EditorUI::ccTouchMoved(touch, event);

        if (g_isInSession && g_sync){
            CCPoint screenPos = touch->getLocation();

            auto editorLayer = LevelEditorLayer::get();
            if (!editorLayer) return;

            auto objLayer = editorLayer->m_objectLayer;
            if (!objLayer) return;

            CCPoint worldPos = objLayer->convertToNodeSpace(screenPos);

            g_sync->onLocalCursorUpdate(worldPos);
        }
    }
};