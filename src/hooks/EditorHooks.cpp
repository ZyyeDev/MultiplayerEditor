#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include <Geode/modify/EditorUI.hpp>

#include "../sync/SyncManager.hpp"

using namespace geode::prelude;

extern SyncManager* g_sync;
extern bool g_isInSession;
extern bool g_isHost;

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

        if (g_isInSession){
            auto selected = this->getSelectedObjects();
            for (auto obj : CCArrayExt<GameObject*>(selected)){
                g_sync->onLocalObjectModified(obj);
            }
        }
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
};