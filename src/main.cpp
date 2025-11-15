#include <Geode/Geode.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>

#include "network/NetworkManager.hpp"
#include "sync/SyncManager.hpp"
#include "ui/Popups.hpp"

using namespace geode::prelude;

// global instances
NetworkManager* g_network = nullptr;
SyncManager* g_sync = nullptr;

// session states
int port = 7777; // this isnt used at all, must be implemented in the future!!
bool g_isHost = false;
bool g_isInSession = false;

bool wasPlaytesting = false;

CCPoint lastMousePos = ccp(0,0);

$on_mod(Loaded){
    if (enet_initialize() != 0) {
        log::error("Failed to initialize ENet!");
        return;
    }
    
    log::info("ENet initialized successfully!");
    log::info("ENet version: {}.{}.{}", 
        ENET_VERSION_MAJOR, 
        ENET_VERSION_MINOR, 
        ENET_VERSION_PATCH
    );

	g_network = new NetworkManager();
	g_sync = new SyncManager();

	log::info("Collab editor loaded!");
}

// Poll in every frame
class $modify(CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt);
        
        if (g_isInSession && g_network) {
            g_network->poll();
        }

        if (g_isInSession && g_sync){
            auto editorLayer = LevelEditorLayer::get();
            if (editorLayer && editorLayer->m_objectLayer){
                if (g_isInSession && g_sync && editorLayer->m_playbackMode == PlaybackMode::Playing) {
                    wasPlaytesting = true;
                    g_sync->updatePlayerSync(dt, editorLayer, false);
                }else if (wasPlaytesting){
                    wasPlaytesting = false;
                    g_sync->sendPlayerPosition(editorLayer, true);
                }
            }
        }
    }
};
