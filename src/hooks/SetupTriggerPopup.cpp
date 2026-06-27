#include <Geode/Geode.hpp>
#include <Geode/modify/SetupTriggerPopup.hpp>
#include <Geode/modify/EditorUI.hpp>

#include "../sync/SyncManager.hpp"
#include "../network/Packets.hpp"

extern SyncManager* g_sync;

class $modify(MySetupTriggerPopup, SetupTriggerPopup) {
    void onClose(CCObject* sender) {
        SetupTriggerPopup::onClose(sender);

        if (!g_sync || g_sync->isApplyingRemoteChanges()) return;

        auto editorLayer = LevelEditorLayer::get();
        auto editorUI = editorLayer ? editorLayer->m_editorUI : nullptr;

        if (editorUI) {
            auto selected = editorUI->getSelectedObjects();
            for (auto obj : CCArrayExt<GameObject*>(selected)){
                if (g_sync->isTrackedObject(obj)) {
                    g_sync->onLocalObjectModified(obj);
                }
            }
        } else if (this->m_gameObject) {
            g_sync->onLocalObjectModified(this->m_gameObject);
        }
    }
};