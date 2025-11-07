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

        if (g_isInSession && g_sync){
            g_sync->onLocalObjectAdded(p0);
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