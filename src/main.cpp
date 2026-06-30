#include <Geode/Geode.hpp>
#include <Geode/loader/Log.hpp>
#include <Geode/modify/EditorUI.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>

#include "network/NetworkManager.hpp"
#include "sync/SyncManager.hpp"
#include "utils/MouseTooltip.hpp"

using namespace geode::prelude;

// global instances
NetworkManager* g_network = nullptr;
SyncManager* g_sync = nullptr;

// session states
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

    g_network->setOnDisconnect([]() {
        if (!g_isHost && g_isInSession) {
            g_isInSession = false;
            
            if (g_sync) {
                g_sync->cleanUpPlayers();
                g_sync->clearAllRemoteState();
            }
            
            log::warn("lost connection to host");
        }
    });

	log::info("Collab editor loaded!");
}

// Poll in every frame
class $modify(CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt);
        MouseTooltip::get()->update();
        
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

            if (
                g_network->requestFullSync &&
                !g_isHost &&
                (g_sync && g_sync->getEditorLayer())
            ){
                g_network->requestFullSync = false;

                FullSyncRequestPacket pkt;

                pkt.header.type = PacketType::FULL_SYNC_REQUEST;
                pkt.header.timestamp = getCurrentTimestamp();
                pkt.header.senderID = g_network->getPeerID();

                g_network->sendPacket(&pkt, sizeof(pkt));
            }
        }
    }
};
