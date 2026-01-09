#include <Geode/Geode.hpp>
#include <Geode/modify/SetupTriggerPopup.hpp>

#include "../sync/SyncManager.hpp"
#include "../network/Packets.hpp"

extern SyncManager* g_sync;

class $modify(MySetupTriggerPopup, SetupTriggerPopup) {
    void onClose(CCObject* sender) {
        SetupTriggerPopup::onClose(sender);

        if (this->m_gameObject) {
            log::info("Trigger properties applied!");
            g_sync->onLocalObjectModified(this->m_gameObject);
        }
    }
};