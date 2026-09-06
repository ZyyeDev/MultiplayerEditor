#include <Geode/Geode.hpp>
#include <sstream>
#include <cstring>
#include <cstddef>
#include <cmath>
#include "SyncManager.hpp"
#include "../network/NetworkManager.hpp"
#include "../network/Packets.hpp"
#include "../utils/MouseTooltip.hpp"

extern NetworkManager* g_network;
extern bool g_isHost;

static PlayerGameMode getLocalPlayerGameMode(PlayerObject* plr) {
    if (plr->m_isShip) return PlayerGameMode::Ship;
    if (plr->m_isBall) return PlayerGameMode::Ball;
    if (plr->m_isBird) return PlayerGameMode::Ufo;
    if (plr->m_isDart) return PlayerGameMode::Wave;
    if (plr->m_isRobot) return PlayerGameMode::Robot;
    if (plr->m_isSpider) return PlayerGameMode::Spider;
    if (plr->m_isSwing) return PlayerGameMode::Swing;
    return PlayerGameMode::Cube;
}

static int iconFrameForMode(PlayerGameMode mode, const PlayerIconData& iconData) {
    switch (mode) {
        case PlayerGameMode::Ship: return iconData.shipID;
        case PlayerGameMode::Ball: return iconData.ballID;
        case PlayerGameMode::Ufo: return iconData.ufoID;
        case PlayerGameMode::Wave: return iconData.waveID;
        case PlayerGameMode::Robot: return iconData.robotID;
        case PlayerGameMode::Spider: return iconData.spiderID;
        case PlayerGameMode::Swing: return iconData.swingID;
        case PlayerGameMode::Jetpack: return iconData.jetpackID;
        default: return iconData.iconID;
    }
}

static void setIconForMode(PlayerObject* player, PlayerGameMode mode, const PlayerIconData& iconData) {
    int frame = iconFrameForMode(mode, iconData);
    switch (mode) {
        case PlayerGameMode::Ship: player->updatePlayerShipFrame(frame); break;
        case PlayerGameMode::Ball: player->updatePlayerRollFrame(frame); break;
        case PlayerGameMode::Ufo: player->updatePlayerBirdFrame(frame); break;
        case PlayerGameMode::Wave: player->updatePlayerDartFrame(frame); break;
        case PlayerGameMode::Robot: player->updatePlayerRobotFrame(frame); break;
        case PlayerGameMode::Spider: player->updatePlayerSpiderFrame(frame); break;
        case PlayerGameMode::Swing: player->updatePlayerSwingFrame(frame); break;
        case PlayerGameMode::Jetpack: player->updatePlayerJetpackFrame(frame); break;
        default: player->updatePlayerFrame(frame); break;
    }
}

static void applyRemotePlayerMode(PlayerObject* player, PlayerGameMode mode, const PlayerIconData& iconData, PlayerGameMode prevMode) {
    if (mode == prevMode) {
        setIconForMode(player, mode, iconData);
        return;
    }

    player->toggleFlyMode(false, true);
    player->toggleRollMode(false, true);
    player->toggleBirdMode(false, true);
    player->toggleDartMode(false, true);
    player->toggleRobotMode(false, true);
    player->toggleSpiderMode(false, true);
    player->toggleSwingMode(false, true);

    switch (mode) {
        case PlayerGameMode::Ship: player->toggleFlyMode(true, true); break;
        case PlayerGameMode::Ball: player->toggleRollMode(true, true); break;
        case PlayerGameMode::Ufo: player->toggleBirdMode(true, true); break;
        case PlayerGameMode::Wave: player->toggleDartMode(true, true); break;
        case PlayerGameMode::Robot: player->toggleRobotMode(true, true); break;
        case PlayerGameMode::Spider: player->toggleSpiderMode(true, true); break;
        case PlayerGameMode::Swing: player->toggleSwingMode(true, true); break;
        default: break;
    }

    setIconForMode(player, mode, iconData);
}

static PlayerObject* createRemotePlayerVisual(LevelEditorLayer* editorLayer, const PlayerIconData& iconData, bool isSecond) {
    auto remotePlayer = PlayerObject::create(
        iconData.iconID,
        iconData.shipID,
        editorLayer,
        editorLayer->m_objectLayer,
        false
    );

    if (!remotePlayer) return nullptr;

    remotePlayer->setOpacity(200);
    remotePlayer->setZOrder(1000);

    auto gameManager = GameManager::sharedState();
    remotePlayer->setColor(gameManager->colorForIdx(iconData.color1ID));
    remotePlayer->setSecondColor(gameManager->colorForIdx(iconData.color2ID));

    if (iconData.hasGlow) {
        remotePlayer->enableCustomGlowColor(gameManager->colorForIdx(iconData.glowColor));
    } else {
        remotePlayer->disableCustomGlowColor();
    }

    if (isSecond) remotePlayer->m_isSecondPlayer = true;

    return remotePlayer;
}

static void updateRemotePlayerVisual(
    PlayerObject* player,
    uint8_t& appliedMode,
    int& appliedFrame,
    PlayerGameMode gameMode,
    const PlayerIconData& iconData,
    float x,
    float y,
    float rotation,
    float playerScale,
    bool upsideDown,
    bool goingLeft,
    bool dead,
    const std::string& robotAnim,
    std::string& appliedRobotAnim
) {
    if (!player) return;

    player->setPosition(ccp(x, y));
    player->setRotation(rotation);
    player->m_isUpsideDown = upsideDown;
    player->m_isGoingLeft = goingLeft;

    int iconFrame = iconFrameForMode(gameMode, iconData);
    if (appliedMode != static_cast<uint8_t>(gameMode) || appliedFrame != iconFrame) {
        applyRemotePlayerMode(player, gameMode, iconData, static_cast<PlayerGameMode>(appliedMode));
        appliedMode = static_cast<uint8_t>(gameMode);
        appliedFrame = iconFrame;
        appliedRobotAnim.clear();
    }

    float scale = (playerScale > 0.01f) ? playerScale : 1.0f;
    player->m_vehicleSize = scale;
    if (player->m_mainLayer) {
        player->m_mainLayer->setScaleX(scale * (goingLeft ? -1.0f : 1.0f));
        player->m_mainLayer->setScaleY(scale * (upsideDown ? -1.0f : 1.0f));
    }

    if (gameMode == PlayerGameMode::Robot && player->m_robotSprite && !robotAnim.empty() && robotAnim != appliedRobotAnim) {
        player->m_robotSprite->runAnimation(robotAnim);
        appliedRobotAnim = robotAnim;
    }

    if (dead) {
        player->m_isDead = true;
        player->setVisible(false);
    } else {
        player->m_isDead = false;
        player->setVisible(true);
    }
}

SyncManager::SyncManager() : m_objectCounter(0), m_lastUpdateTimestamp(0) {
    m_userID = g_network->getPeerID();

    g_network->setOnRecive([this](const uint8_t* data, size_t size){
        this->handlePacket(data, size);
    });
}

std::string SyncManager::generateUID() {
    return std::to_string(m_userID) + "_" + std::to_string(m_objectCounter++);
}

void SyncManager::trackObject(const std::string& uid, GameObject* obj){
    m_syncedObjects[uid] = obj;
    m_objectToUID[obj] = uid;
}

void SyncManager::untrackObject(const std::string& uid){
    auto thing = m_syncedObjects.find(uid);
    if (thing != m_syncedObjects.end()){
        m_objectToUID.erase(thing->second);
        m_syncedObjects.erase(thing);
    }
}

GameObject* SyncManager::createAndDiffObjectFromString(LevelEditorLayer* editor, const std::string& objString){
    if (!editor) return nullptr;

    std::unordered_set<GameObject*> before;
    if (editor->m_objects) {
        before.reserve(editor->m_objects->count());
        for (int i = 0; i < editor->m_objects->count(); i++) {
            before.insert(static_cast<GameObject*>(editor->m_objects->objectAtIndex(i)));
        }
    }

    editor->createObjectsFromString(objString, false, true);

    if (!editor->m_objects) return nullptr;
    for (int i = 0; i < editor->m_objects->count(); i++) {
        auto obj = static_cast<GameObject*>(editor->m_objects->objectAtIndex(i));
        if (before.find(obj) == before.end()) {
            return obj;
        }
    }
    return nullptr;
}

bool SyncManager::isTrackedObject(GameObject* obj){
    return m_objectToUID.find(obj) != m_objectToUID.end();
}

std::string SyncManager::getObjectUid(GameObject* obj){
    auto thing = m_objectToUID.find(obj);
    if (thing != m_objectToUID.end()){
        return thing->second;
    }else{
        return "";
    }
}

LevelEditorLayer* SyncManager::getEditorLayer(){
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return nullptr;
    
    return scene->getChildByType<LevelEditorLayer>(0);
}

void SyncManager::sendObjectPackets(PacketType type, const std::string& uid, const std::string& objString) {
    const size_t chunkSize = sizeof(ObjectStringPacket::objectString);
    uint32_t chunkIndex = 0;
    size_t offset = 0;

    while (offset < objString.size() || chunkIndex == 0) {
        size_t remaining = objString.size() - offset;
        size_t thisChunkLen = std::min(remaining, chunkSize);
        bool hasMore = (offset + thisChunkLen) < objString.size();

        ObjectStringPacket packet;
        packet.header.type = type;
        packet.header.timestamp = getCurrentTimestamp();
        packet.header.senderID = g_network->getPeerID();
        strncpy(packet.uid, uid.c_str(), 31);
        packet.uid[31] = '\0';
        packet.chunkIndex = chunkIndex;
        packet.chunkLength = (uint32_t)thisChunkLen;
        packet.hasMore = hasMore;
        memcpy(packet.objectString, objString.c_str() + offset, thisChunkLen);
        if (thisChunkLen < chunkSize) packet.objectString[thisChunkLen] = '\0';

        g_network->sendPacket(&packet, offsetof(ObjectStringPacket, objectString) + thisChunkLen);

        offset += thisChunkLen;
        chunkIndex++;

        if (!hasMore) break;
    }
}

void SyncManager::onLocalObjectAdded(GameObject* obj) {
    std::string uid = generateUID();
    trackObject(uid, obj);
    m_localObjects.insert(obj);
    
    auto editor = getEditorLayer();
    gd::string gdString = obj->getSaveString(editor);
    std::string objString = std::string(gdString);
    
    sendObjectPackets(PacketType::OBJECT_ADD, uid, objString);
}


void SyncManager::onLocalObjectDestroyed(GameObject* obj) {
    if (!isTrackedObject(obj)) return;
    
    std::string uid = getObjectUid(obj);
    
    for (auto& [userId, highlights] : m_remoteSelectionHighlights) {
        highlights.erase(std::remove_if(highlights.begin(), highlights.end(), [obj](CCSprite* sprite){
            return sprite && sprite->getParent() == obj;
        }), highlights.end());
    }
    MouseTooltip::get()->unregisterRegion(obj);

    ObjectDeletePacket packet;
    packet.header.type = PacketType::OBJECT_DELETE;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    strncpy(packet.uid, uid.c_str(), 31);
    packet.uid[31] = '\0';
    
    g_network->sendPacket(&packet, sizeof(packet));
    m_localObjects.erase(obj);
    untrackObject(uid);
}

void SyncManager::onLocalObjectModified(GameObject* obj) {
    if (!isTrackedObject(obj)) return;
    
    std::string uid = getObjectUid(obj);
    
    auto editor = getEditorLayer();
    gd::string gdString = obj->getSaveString(editor);
    std::string objString = std::string(gdString);
    
    sendObjectPackets(PacketType::OBJECT_UPDATE, uid, objString);
}

void SyncManager::onRemoteObjectAdded(const std::string& uid, const std::string& objString) {
    auto editor = getEditorLayer();
    if (!editor) {
        log::error("no editor layer!");
        return;
    }
    
    m_applyingRemoteChanges = true;

    GameObject* newObj = createAndDiffObjectFromString(editor, objString);

    if (newObj) {
        trackObject(uid, newObj);
        log::info("created object: {}", uid);
    } else {
        log::error("object creation failed for uid: {}", uid);
    }

    m_applyingRemoteChanges = false;
}

void SyncManager::onRemoteObjectDestroyed(const ObjectDeletePacket& packet) {
    auto it = m_syncedObjects.find(packet.uid);
    if (it == m_syncedObjects.end()) return;
    
    auto editor = getEditorLayer();
    if (!editor) return;
    
    GameObject* obj = it->second;

    for (auto& [userId, highlights] : m_remoteSelectionHighlights) {
        highlights.erase(std::remove_if(highlights.begin(), highlights.end(), [obj](CCSprite* sprite){
            return sprite && sprite->getParent() == obj;
        }), highlights.end());
    }
    MouseTooltip::get()->unregisterRegion(obj);

    m_localObjects.erase(obj);
    untrackObject(packet.uid);
    
    m_applyingRemoteChanges = true;
    if (editor->m_editorUI) {
        if (editor->m_editorUI->m_selectedObject == obj) {
            editor->m_editorUI->m_selectedObject = nullptr;
        }
        if (editor->m_editorUI->m_selectedObjects && editor->m_editorUI->m_selectedObjects->containsObject(obj)) {
            editor->m_editorUI->deselectObject(obj);
            editor->m_editorUI->m_selectedObjects->removeObject(obj);
        }
    }
    if (editor->m_objects && editor->m_objects->containsObject(obj)) {
        editor->removeObject(obj, false);
    }
    m_applyingRemoteChanges = false;
}

void SyncManager::onRemoteObjectModified(const std::string& uid, const std::string& objString) {
    auto it = m_syncedObjects.find(uid);
    
    if (it == m_syncedObjects.end()) return;
    
    GameObject* oldObj = it->second;
    if (!oldObj) return;

    auto editor = getEditorLayer();
    if (!editor || !editor->m_objects) return;
    
    m_applyingRemoteChanges = true;

    for (auto& [userId, highlights] : m_remoteSelectionHighlights) {
        highlights.erase(std::remove_if(highlights.begin(), highlights.end(), [oldObj](CCSprite* sprite){
            return sprite && sprite->getParent() == oldObj;
        }), highlights.end());
    }
    MouseTooltip::get()->unregisterRegion(oldObj);

    m_localObjects.erase(oldObj);
    if (editor->m_editorUI) {
        if (editor->m_editorUI->m_selectedObject == oldObj) {
            editor->m_editorUI->m_selectedObject = nullptr;
        }
        if (editor->m_editorUI->m_selectedObjects && editor->m_editorUI->m_selectedObjects->containsObject(oldObj)) {
            editor->m_editorUI->deselectObject(oldObj);
            editor->m_editorUI->m_selectedObjects->removeObject(oldObj);
        }
    }
    if (editor->m_objects->containsObject(oldObj)) {
        editor->removeObject(oldObj, false);
    }
    untrackObject(uid);

    GameObject* newObj = createAndDiffObjectFromString(editor, objString);

    if (newObj) {
        trackObject(uid, newObj);
    } else {
        log::error("object update failed for uid: {}", uid);
    }
    
    m_applyingRemoteChanges = false;
}

void SyncManager::sendFullState(uint32_t targetPeerID) {
    auto editor = getEditorLayer();
    if (!editor) {
        log::warn("no editor layer {}", targetPeerID);
        return;
    }

    auto allObjects = editor->m_objects;
    if (!allObjects) {
        log::warn("editor has no objects array {}", targetPeerID);
        return;
    }

    log::info("sending {} objects to peer {}", allObjects->count(), targetPeerID);

    for (int i = 0; i < allObjects->count(); i++) {
        auto obj = static_cast<GameObject*>(allObjects->objectAtIndex(i));

        if (!isTrackedObject(obj)) {
            std::string uid = generateUID();
            trackObject(uid, obj);
        }

        gd::string gdString = obj->getSaveString(editor);
        std::string objString = std::string(gdString);
        std::string uid = getObjectUid(obj);

        const size_t chunkSize = sizeof(ObjectStringPacket::objectString);
        uint32_t chunkIndex = 0;
        size_t offset = 0;

        while (offset < objString.size() || chunkIndex == 0) {
            size_t remaining = objString.size() - offset;
            size_t thisChunkLen = std::min(remaining, chunkSize);
            bool hasMore = (offset + thisChunkLen) < objString.size();

            ObjectStringPacket pkt;
            pkt.header.type = PacketType::OBJECT_ADD;
            pkt.header.timestamp = getCurrentTimestamp();
            pkt.header.senderID = g_network->getPeerID();
            strncpy(pkt.uid, uid.c_str(), 31);
            pkt.uid[31] = '\0';
            pkt.chunkIndex = chunkIndex;
            pkt.chunkLength = (uint32_t)thisChunkLen;
            pkt.hasMore = hasMore;
            memcpy(pkt.objectString, objString.c_str() + offset, thisChunkLen);
            if (thisChunkLen < chunkSize) pkt.objectString[thisChunkLen] = '\0';

            g_network->sendPacketToPeer(targetPeerID, &pkt, offsetof(ObjectStringPacket, objectString) + thisChunkLen);

            offset += thisChunkLen;
            chunkIndex++;
            if (!hasMore) break;
        }

    }

    sendAllColors(targetPeerID);
    onLocalLevelSettingsChanged();

    FullSyncEndPacket endPkt;
    endPkt.header.type = PacketType::FULL_SYNC_END;
    endPkt.header.timestamp = getCurrentTimestamp();
    endPkt.header.senderID = g_network->getPeerID();
    g_network->sendPacketToPeer(targetPeerID, &endPkt, sizeof(endPkt));

    log::info("Sent full state ({} objects) to peer {}", allObjects->count(), targetPeerID);
}

void SyncManager::handlePacket(const uint8_t* data, size_t size) {
    if (size < sizeof(PacketHeader)) return;
    
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);

    if (g_isHost && header->senderID != g_network->getPeerID()) {
        switch (header->type) {
            case PacketType::OBJECT_ADD:
            case PacketType::OBJECT_DELETE:
            case PacketType::OBJECT_UPDATE:
            case PacketType::MOUSE_MOVE:
            case PacketType::SELECT_CHANGE:
            case PacketType::COLOR_SYNC:
            case PacketType::PLAYER_POSITION:
                g_network->relayPacket(header->senderID, data, size);
                break;
            default:
                break;
        }
    }
    
    switch (header->type) {
        case PacketType::HANDSHAKE: {
            if (size < sizeof(HandshakePacket)) break;
            const HandshakePacket* packet = reinterpret_cast<const HandshakePacket*>(data);
            std::string theirVersion = safeStr(packet->version);
            std::string theirUsername = safeStr(packet->username);
            // i think we want to do this in another way but it works for now
            std::string myVersion = Mod::get()->getVersion().toNonVString();
            log::info("got handshake from {}, their version '{}' my version '{}'", packet->header.senderID, theirVersion, myVersion);
            
            log::info("registering peer {} as '{}'", packet->header.senderID, theirUsername);
            g_network->addPeer(packet->header.senderID, theirUsername);
            if (g_isHost){
                g_network->broadcastPeerJoined(packet->header.senderID, theirUsername);

                // we MUST do this in another way
                if (!g_network->checkPassword(safeStr(packet->password))){
                    log::info("user connecting password is incorrect");
                    
                    KickPacket _kickPacket;
                    _kickPacket.header.type = PacketType::KICK_USER;
                    _kickPacket.header.timestamp = getCurrentTimestamp();
                    _kickPacket.header.senderID = g_network->getPeerID();
                    _kickPacket.userToKick = packet->header.senderID;
                    strncpy(_kickPacket.kickReason, "Wrong Password", 127);
                    _kickPacket.kickReason[127] = '\0';
                    g_network->sendPacketToPeer(
                        packet->header.senderID,
                        &_kickPacket,
                        sizeof(_kickPacket)
                    );
                    break;
                }

                if (myVersion != theirVersion){
                    log::warn("version mismatch, kicking {}", packet->header.senderID);
                    if (g_isHost){
                        KickPacket _kickPacket;
                        _kickPacket.header.type = PacketType::KICK_USER;
                        _kickPacket.header.timestamp = getCurrentTimestamp();
                        _kickPacket.header.senderID = g_network->getPeerID();
                        _kickPacket.userToKick = packet->header.senderID;
                        strncpy(_kickPacket.kickReason, "Version Mismatch", 127);
                        _kickPacket.kickReason[127] = '\0';
                        g_network->sendPacketToPeer(
                            packet->header.senderID,
                            &_kickPacket,
                            sizeof(_kickPacket)
                        );
                    }
                    break;
                }

                g_network->sendLobbyState(packet->header.senderID);
            }
            break;
        }
        case PacketType::KICK_USER: {
            if (size < sizeof(KickPacket)) break;
            const KickPacket* packet = reinterpret_cast<const KickPacket*>(data);
            if (!g_isHost && packet->userToKick == g_network->getPeerID()){
                g_network->gotKicked(safeStr(packet->kickReason));
                break;
            }
            g_network->removePeer(packet->userToKick);
            m_remoteSelections[packet->userToKick].clear();
            onRemoteSelectionChanged(packet->userToKick);
            m_remoteSelections.erase(packet->userToKick);
            m_remoteSelectionHighlights.erase(packet->userToKick);
            {
                auto cursorIt = m_remoteCursors.find(packet->userToKick);
                if (cursorIt != m_remoteCursors.end()) {
                    if (cursorIt->second && cursorIt->second->getParent()) {
                        cursorIt->second->removeFromParent();
                    }
                    m_remoteCursors.erase(cursorIt);
                }
            }
            log::info("peer left {}", packet->userToKick);
            break;
        }
        case PacketType::PEER_JOINED: {
            if (size < sizeof(PeerJoinedPacket)) break;
            const PeerJoinedPacket* packet = reinterpret_cast<const PeerJoinedPacket*>(data);
            std::string theirUsername = safeStr(packet->username);
            g_network->addPeer(packet->peerID, theirUsername);
            log::info("peer joined {} ({})", packet->peerID, theirUsername);
            break;
        }
        case PacketType::PEER_LEFT: {
            if (size < sizeof(PeerLeftPacket)) break;
            const PeerLeftPacket* packet = reinterpret_cast<const PeerLeftPacket*>(data);
            g_network->removePeer(packet->peerID);
            m_remoteSelections[packet->peerID].clear();
            onRemoteSelectionChanged(packet->peerID);
            m_remoteSelections.erase(packet->peerID);
            m_remoteSelectionHighlights.erase(packet->peerID);
            {
                auto cursorIt = m_remoteCursors.find(packet->peerID);
                if (cursorIt != m_remoteCursors.end()) {
                    if (cursorIt->second && cursorIt->second->getParent()) {
                        cursorIt->second->removeFromParent();
                    }
                    m_remoteCursors.erase(cursorIt);
                }
            }
            log::info("peer left {}", packet->peerID);
            break;
        }
        case PacketType::LOBBY_SYNC: {
            if (size < sizeof(LobbySyncPacket)) break;
            const LobbySyncPacket* packet = reinterpret_cast<const LobbySyncPacket*>(data);

            g_network->m_peersInLobby.clear();

            for (uint32_t i = 0; i < packet->memberCount && i < maxPlayers; i++){
                g_network->addPeer(
                    packet->members[i].peerID,
                    packet->members[i].username
                );
            }

            log::info("lobby synced: {} members", packet->memberCount);
            break;
        }
        case PacketType::FULL_SYNC_REQUEST: {
            log::info("got full sync request from {}, am i host: {}", header->senderID, g_isHost);
            if (g_isHost) {
                sendFullState(header->senderID);
            }
            break;
        }
        case PacketType::FULL_SYNC_END: {
            m_localObjects.clear();
            trackExistingObjects();
            auto editor = getEditorLayer();
            if (editor && editor->m_editorUI) {
                editor->m_editorUI->updateButtons();
            }
            break;
        }
        case PacketType::OBJECT_ADD: {
            if (size < offsetof(ObjectStringPacket, objectString)) break;
            const ObjectStringPacket* packet = reinterpret_cast<const ObjectStringPacket*>(data);
            if (size < offsetof(ObjectStringPacket, objectString) + packet->chunkLength) break;
            std::string uid = safeStr(packet->uid);

            if (packet->chunkIndex == 0) m_incomingChunks[uid] = ChunkBuffer{};
            m_incomingChunks[uid].data.append(packet->objectString, packet->chunkLength);
            m_incomingChunks[uid].lastChunkIndex = packet->chunkIndex;

            if (!packet->hasMore) {
                std::string fullString = m_incomingChunks[uid].data;
                m_incomingChunks.erase(uid);
                onRemoteObjectAdded(uid, fullString);
            }
            break;
        }
        case PacketType::OBJECT_DELETE: {
            if (size < sizeof(ObjectDeletePacket)) break;
            const ObjectDeletePacket* packet = reinterpret_cast<const ObjectDeletePacket*>(data);
            onRemoteObjectDestroyed(*packet);
            break;
        }
        case PacketType::OBJECT_UPDATE: {
            if (size < offsetof(ObjectStringPacket, objectString)) break;
            const ObjectStringPacket* packet = reinterpret_cast<const ObjectStringPacket*>(data);
            if (size < offsetof(ObjectStringPacket, objectString) + packet->chunkLength) break;
            std::string uid = safeStr(packet->uid);

            if (packet->chunkIndex == 0) m_incomingChunks[uid] = ChunkBuffer{};
            m_incomingChunks[uid].data.append(packet->objectString, packet->chunkLength);
            m_incomingChunks[uid].lastChunkIndex = packet->chunkIndex;

            if (!packet->hasMore) {
                std::string fullString = m_incomingChunks[uid].data;
                m_incomingChunks.erase(uid);
                
                m_pendingObjectUpdates[uid] = fullString;
            }
            break;
        }
        case PacketType::MOUSE_MOVE: {
            if (size < sizeof(MousePacket)) break;
            const MousePacket* packet = reinterpret_cast<const MousePacket*>(data);
            onRemoteCursorUpdate(packet->header.senderID, packet->x, packet->y);
            break;
        }
        case PacketType::COLOR_SYNC: {
            if (size < offsetof(ColorChannelsPacket, colorDat)) break;
            const ColorChannelsPacket* packet = reinterpret_cast<const ColorChannelsPacket*>(data);
            if (packet->count > 1000) break;
            size_t expectedSize = offsetof(ColorChannelsPacket, colorDat) + packet->count * sizeof(SavedColorData);
            if (size < expectedSize) break;
            std::vector<SavedColorData> colors(packet->colorDat, packet->colorDat + packet->count);

            for (auto i : colors)
            {
                restoreColor(i);
            }
            
            break;
        }
        case PacketType::SELECT_CHANGE: {
            if (size < sizeof(SelectPacket)) break;
            const SelectPacket* packet = reinterpret_cast<const SelectPacket*>(data);
            
            if (packet->chunkIndex == 0) {
                m_remoteSelections[packet->header.senderID].clear();
            }
            
            for (uint32_t i = 0; i < packet->countInChunk && i < 50; i++) {
                std::string uid = safeStr(packet->uids[i]);
                m_remoteSelections[packet->header.senderID][uid] = 3.0f;
            }
            
            if (!packet->hasMore) {
                onRemoteSelectionChanged(packet->header.senderID);
            }
            
            break;
        }
        case PacketType::LEVEL_SETTINGS: {
            if (size < sizeof(LevelSettingsPacket)) break;
            const LevelSettingsPacket* packet = reinterpret_cast<const LevelSettingsPacket*>(data);
            onRemoteLevelSettingsChanged(*packet);
            break;
        }
        case PacketType::PLAYER_POSITION: {
            if (size < sizeof(PlayerPositionPacket)) break;
            const PlayerPositionPacket* packet = reinterpret_cast<const PlayerPositionPacket*>(data);
            
            auto editorLayer = getEditorLayer();
            if (editorLayer){
                onRemotePlayerPosition(*packet, editorLayer);
            }else{
                log::error("editor layer does not exist!");
            }

            break;
        }
        default:
            log::warn("Unknown packet type: {}", (int)header->type);
            break;
    }
}

bool SyncManager::shouldApplyUpdate(uint32_t remoteTimestamp) {
    if (remoteTimestamp >= m_lastUpdateTimestamp) {
        m_lastUpdateTimestamp = remoteTimestamp;
        return true;
    }
    return false;
}

void SyncManager::onLocalCursorUpdate(CCPoint position){
    float distance = ccpDistance(m_CursorPos, position);
    if (distance < 0.5f) return;

    m_CursorPos = position;
    
    MousePacket packet;
    packet.header.type = PacketType::MOUSE_MOVE;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    
    packet.x = position.x;
    packet.y = position.y;
    
    g_network->sendPacket(&packet, sizeof(packet));
}

ccColor3B SyncManager::colorForUser(uint32_t userID){
    static const ccColor3B palette[] = {
        {255, 80, 80},
        {80, 160, 255},
        {255, 200, 60},
        {130, 255, 130},
        {255, 120, 255},
        {80, 255, 220},
        {255, 160, 80},
        {180, 140, 255},
    };

    return palette[userID % (sizeof(palette) / sizeof(palette[0]))];
}

void SyncManager::onRemoteCursorUpdate(const uint32_t& userID, int x, int y){
    CCPoint position = ccp(x, y);

    auto it = m_remoteCursors.find(userID);
    if (it == m_remoteCursors.end()){
        auto editor = getEditorLayer();
        if (!editor || !editor->m_objectLayer) return;

        auto cursor = CCSprite::create("cursor.png"_spr);
        if (!cursor) return;
        
        cursor->setZOrder(INT_MAX);
        cursor->setPosition(position);
        cursor->setColor(colorForUser(userID));

        auto peers = g_network->m_peersInLobby;
        auto nameIt = peers.find(userID);
        if (nameIt != peers.end()) {
            auto label = CCLabelBMFont::create(nameIt->second.c_str(), "chatFont.fnt");
            label->setScale(0.45f);
            label->setPosition(ccp(
                cursor->getContentSize().width / 2.f,
                cursor->getContentSize().height + 6.f
            ));
            label->setZOrder(1);
            label->setColor(colorForUser(userID));
            cursor->addChild(label);
        }

        editor->m_objectLayer->addChild(cursor);
        m_remoteCursors[userID] = cursor;
    } else {
        it->second->setPosition(position);
    }
}

bool SyncManager::isObjectLockedByOther(GameObject* obj, uint32_t* outUserID){
    if (!isTrackedObject(obj)) return false;

    std::string uid = getObjectUid(obj);
    uint32_t myID = g_network->getPeerID();

    for (auto& [userId, selection] : m_remoteSelections){
        if (userId == myID) continue;

        auto it = selection.find(uid);
        if (it != selection.end() && it->second > 0.f){
            if (outUserID) *outUserID = userId;
            return true;
        }
    }

    return false;
}

void SyncManager::updateLocks(float dt){
    flushPendingObjectUpdates();

    for (auto& [userId, selection] : m_remoteSelections){
        std::vector<std::string> expired;
        for (auto& [uid, ttl] : selection){
            ttl -= dt;
            if (ttl <= 0.f) expired.push_back(uid);
        }
        bool changed = !expired.empty();
        for (auto& uid : expired) selection.erase(uid);
        if (changed) onRemoteSelectionChanged(userId);
    }
}

void SyncManager::flushPendingObjectUpdates(){
    if (m_pendingObjectUpdates.empty()) return;

    auto batch = std::move(m_pendingObjectUpdates);
    m_pendingObjectUpdates.clear();

    for (auto& [uid, objString] : batch){
        onRemoteObjectModified(uid, objString);
    }
}

void SyncManager::onLocalSelectionChanged(CCArray* selectedObjects){
    if (!selectedObjects){
        log::error("selected objects is null!!");
        return;
    }

    std::vector<std::string> uids;
    for (auto obj : CCArrayExt<GameObject*>(selectedObjects)){
        if (isTrackedObject(obj)){
            uids.push_back(getObjectUid(obj));
        }
    }
    
    uint32_t totalCount = uids.size();
    uint32_t chunkIndex = 0;

    for (size_t i = 0; i < uids.size(); i += 50){
        SelectPacket packet;
        packet.header.type = PacketType::SELECT_CHANGE;
        packet.header.timestamp = getCurrentTimestamp();
        packet.header.senderID = g_network->getPeerID();

        packet.chunkIndex = chunkIndex++;
        packet.totalCount = totalCount;

        uint32_t countInChunk = std::min((size_t)50, uids.size() - i);
        packet.countInChunk = countInChunk;
        packet.hasMore = i + 50 < uids.size();

        for (uint32_t j = 0; j < countInChunk; j++){
            strcpy(
                packet.uids[j],
                uids[i + j].c_str()
            );
        }
        g_network->sendPacket(&packet, sizeof(packet));
    }
    
    if (uids.empty()){
        SelectPacket packet;
        packet.header.type = PacketType::SELECT_CHANGE;
        packet.header.timestamp = getCurrentTimestamp();
        packet.header.senderID = g_network->getPeerID();
        packet.chunkIndex = 0;
        packet.totalCount = 0;
        packet.countInChunk = 0;
        packet.hasMore = false;
        
        g_network->sendPacket(&packet, sizeof(packet));
    }
}

void SyncManager::onRemoteSelectionChanged(const uint32_t& userID){
    if (!m_remoteSelections.contains(userID)){
        return;
    }
    
    // remove old highlights
    if (m_remoteSelectionHighlights.contains(userID)) {
        auto& highlights = m_remoteSelectionHighlights[userID];
        for (auto sprite : highlights){
            if (sprite){
                MouseTooltip::get()->unregisterRegion(sprite->getParent());
                sprite->removeFromParent();
            }
        }
        highlights.clear();
    }

    auto editor = getEditorLayer();
    if (!editor) return;
    if (!editor->m_objectLayer) return;

    auto& selection = m_remoteSelections[userID];
    auto peers = g_network->m_peersInLobby;
    auto nameIt = peers.find(userID);
    std::string username = nameIt != peers.end() ? nameIt->second.c_str() : "someone";

    for (const auto& [uid, ttl] : selection){
        if (ttl <= 0.f) continue;
        auto it = m_syncedObjects.find(uid);
        if (it == m_syncedObjects.end()) continue;

        GameObject* obj = it->second;

        auto highlight = CCSprite::createWithSpriteFrameName("whiteSquare60_001.png");
        if (!highlight){
            continue;
        }

        highlight->setColor(colorForUser(userID));
        highlight->setOpacity(128);

        CCSize objSize = obj->getContentSize();
        highlight->setScaleX(objSize.width / highlight->getContentSize().width);
        highlight->setScaleY(objSize.height / highlight->getContentSize().height);
        
        highlight->setAnchorPoint(obj->getAnchorPoint());

        CCPoint anchorPoint = obj->getAnchorPoint();
        highlight->setPosition(ccp(objSize.width * anchorPoint.x, objSize.height * anchorPoint.y));
        
        highlight->setZOrder(obj->getZOrder() - 1);
        
        obj->addChild(highlight);
        m_remoteSelectionHighlights[userID].push_back(highlight);

        MouseTooltip::get()->registerRegion(obj, username + " is editing this", colorForUser(userID));
    }
}

std::string SyncManager::extractSettingsString() {
    auto editor = getEditorLayer();
    if (!editor || !editor->m_levelSettings) return "";
    gd::string gs = editor->m_levelSettings->getSaveString();
    return std::string(gs);
}

bool SyncManager::isPopupBlockingLevelSettings() {
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return false;

    if (auto transition = typeinfo_cast<CCTransitionScene*>(scene)) {
        scene = transition->m_pInScene ? transition->m_pInScene : scene;
    }

    for (auto child : CCArrayExt<CCNode*>(scene->getChildren())) {
        if (typeinfo_cast<FLAlertLayer*>(child)) return true;
        if (typeinfo_cast<GJDropDownLayer*>(child)) return true;
    }

    return false;
}

void SyncManager::onRemoteLevelSettingsChanged(const LevelSettingsPacket& packet) {
    if (g_isHost) return;

    if (isPopupBlockingLevelSettings()) {
        m_pendingLevelSettings = packet;
        m_hasPendingLevelSettings = true;
        return;
    }

    applyLevelSettings(packet);
}

void SyncManager::processPendingLevelSettings() {
    if (!m_hasPendingLevelSettings) return;
    if (isPopupBlockingLevelSettings()) return;

    m_hasPendingLevelSettings = false;
    applyLevelSettings(m_pendingLevelSettings);
}

void SyncManager::applyLevelSettings(const LevelSettingsPacket& settings) {
    auto editor = getEditorLayer();
    if (!editor) return;

    m_applyingRemoteChanges = true;

    std::string saveStr(settings.settingsString, settings.settingsLength);

    if (!saveStr.empty() && editor->m_levelSettings) {
        auto* newSettings = LevelSettingsObject::objectFromString(saveStr);
        if (newSettings) {
            editor->m_levelString = saveStr;

            editor->m_levelSettings->m_startMode = newSettings->m_startMode;
            editor->m_levelSettings->m_startSpeed = newSettings->m_startSpeed;
            editor->m_levelSettings->m_startMini = newSettings->m_startMini;
            editor->m_levelSettings->m_startDual = newSettings->m_startDual;
            editor->m_levelSettings->m_twoPlayerMode = newSettings->m_twoPlayerMode;
            editor->m_levelSettings->m_isFlipped = newSettings->m_isFlipped;
            editor->m_levelSettings->m_songOffset = newSettings->m_songOffset;

            editor->updateOptions();
        }
    }

    if (editor->m_level) {
        bool songChanged = (editor->m_level->m_songID != settings.songID
            || editor->m_level->m_audioTrack != settings.audioTrack);
        editor->m_level->m_audioTrack = settings.audioTrack;
        editor->m_level->m_songID = settings.songID;
        editor->m_level->m_levelLength = settings.levelLength;

        if (songChanged) {
            if (settings.songID > 0) {
                if (!MusicDownloadManager::sharedState()->isSongDownloaded(settings.songID)) {
                    geode::Notification::create("Downloading song", geode::NotificationIcon::Info)->show();
                    MusicDownloadManager::sharedState()->downloadCustomSong(settings.songID);
                }
            }

            auto* engine = FMODAudioEngine::sharedEngine();
            engine->stopAllMusic(false);
        }
    }

    editor->levelSettingsUpdated();

    m_applyingRemoteChanges = false;
}

void SyncManager::onLocalLevelSettingsChanged() {
    auto editorLayer = LevelEditorLayer::get();
    if (!editorLayer || !editorLayer->m_levelSettings) return;

    LevelSettingsPacket packet;
    packet.header.type = PacketType::LEVEL_SETTINGS;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();

    std::string settingsStr = extractSettingsString();
    packet.settingsLength = (uint32_t)std::min(settingsStr.size(), sizeof(packet.settingsString) - 1);
    memcpy(packet.settingsString, settingsStr.c_str(), packet.settingsLength);
    packet.settingsString[packet.settingsLength] = '\0';

    packet.audioTrack = 0;
    packet.songID = 0;
    packet.levelLength = 0;
    if (editorLayer->m_level) {
        packet.audioTrack = editorLayer->m_level->m_audioTrack;
        packet.songID = editorLayer->m_level->m_songID;
        packet.levelLength = editorLayer->m_level->m_levelLength;
    }

    g_network->sendPacket(&packet, sizeof(packet));
}

void SyncManager::trackExistingObjects(){
    auto editor = getEditorLayer();
    if (!editor) return;

    auto allObjects = editor->m_objects;
    if (!allObjects) return;

    for (auto obj : CCArrayExt<GameObject*>(allObjects)) {
        if (!isTrackedObject(obj)){
            std::string uid = generateUID();
            trackObject(uid, obj);
        }
    }
}

void SyncManager::pruneStaleTrackedObjects() {
    auto editor = getEditorLayer();
    if (!editor || !editor->m_objects) return;

    std::unordered_set<GameObject*> editorObjects;
    editorObjects.reserve(editor->m_objects->count());
    for (int i = 0; i < editor->m_objects->count(); i++) {
        editorObjects.insert(static_cast<GameObject*>(editor->m_objects->objectAtIndex(i)));
    }

    std::vector<std::string> stale;
    for (auto& [uid, obj] : m_syncedObjects) {
        if (!editorObjects.count(obj)) {
            stale.push_back(uid);
        }
    }

    if (stale.empty()) return;

    for (auto& uid : stale) {
        m_localObjects.erase(m_syncedObjects[uid]);
        untrackObject(uid);

        for (auto& [userId, selection] : m_remoteSelections) {
            selection.erase(uid);
        }
    }

    for (auto& [userId, selection] : m_remoteSelections) {
        onRemoteSelectionChanged(userId);
    }
}

void SyncManager::syncAfterUndoRedo() {
    auto editor = getEditorLayer();
    if (!editor || !editor->m_objects) return;

    std::unordered_set<GameObject*> editorObjects;
    editorObjects.reserve(editor->m_objects->count());
    for (int i = 0; i < editor->m_objects->count(); i++) {
        editorObjects.insert(static_cast<GameObject*>(editor->m_objects->objectAtIndex(i)));
    }

    // removed by undo
    std::vector<std::string> removed;
    for (auto& [uid, obj] : m_syncedObjects) {
        if (!editorObjects.count(obj)) {
            ObjectDeletePacket pkt;
            pkt.header.type = PacketType::OBJECT_DELETE;
            pkt.header.timestamp = getCurrentTimestamp();
            pkt.header.senderID = g_network->getPeerID();
            strncpy(pkt.uid, uid.c_str(), 31);
            pkt.uid[31] = '\0';
            g_network->sendPacket(&pkt, sizeof(pkt));
            removed.push_back(uid);
        }
    }
    for (auto& uid : removed) {
        m_localObjects.erase(m_syncedObjects[uid]);
        untrackObject(uid);

        for (auto& [userId, selection] : m_remoteSelections) {
            selection.erase(uid);
        }
    }

    if (!removed.empty()) {
        for (auto& [userId, selection] : m_remoteSelections) {
            onRemoteSelectionChanged(userId);
        }
    }

    // restored by undo
    for (auto obj : CCArrayExt<GameObject*>(editor->m_objects)) {
        if (!isTrackedObject(obj)) {
            onLocalObjectAdded(obj);
        }
    }

    if (editor->m_editorUI) {
        auto selected = editor->m_editorUI->getSelectedObjects();
        if (selected) {
            for (auto obj : CCArrayExt<GameObject*>(selected)) {
                if (isTrackedObject(obj)) {
                    onLocalObjectModified(obj);
                }
            }
        }
    }
}

void SyncManager::updatePlayerSync(float dt, LevelEditorLayer* editorLayer, bool stopPlaytest){
    if (!editorLayer) return;
    
    auto plr = editorLayer->m_player1;
    if (!plr){
        log::error("plr not found!!!");
        return;
    }

    m_lastPlayerSendTime += dt;

    if (m_lastPlayerSendTime >= 0.05f){
        m_lastPlayerSendTime = 0.0f;
        sendPlayerPosition(editorLayer, stopPlaytest);
    }
    
    for (auto& [userId, remotePlr] : m_remotePlayers){
        if (remotePlr.player && !remotePlr.player->m_isDead){
            remotePlr.player->setVisible(true);
        }
        if (remotePlr.player2 && !remotePlr.player2->m_isDead){
            remotePlr.player2->setVisible(true);
        }
    }
}

void SyncManager::sendPlayerPosition(LevelEditorLayer* editorLayer, bool stopPlaytest){
    if (!editorLayer) return;
    
    auto plr = editorLayer->m_player1;
    if (!plr){
        log::error("plr not found while sending pos!!!");
        return;
    }

    auto gameManager = GameManager::sharedState();
    if (!gameManager) return;

    PlayerPositionPacket packet;
    packet.header.type = PacketType::PLAYER_POSITION;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();

    packet.x = plr->getPositionX();
    packet.y = plr->getPositionY();
    packet.rotation = plr->getRotation();
    packet.playerScale = plr->m_vehicleSize > 0.01f ? plr->m_vehicleSize : 1.0f;
    packet.isUpsideDown = plr->m_isUpsideDown;
    packet.isGoingLeft = plr->m_isGoingLeft;
    packet.isDead = plr->m_isDead;
    packet.stopPlaytest = stopPlaytest;
    packet.gameMode = static_cast<uint8_t>(getLocalPlayerGameMode(plr));

    packet.robotAnim[0] = '\0';
    if (plr->m_isRobot && !plr->m_currentRobotAnimation.empty()) {
        strncpy(packet.robotAnim, plr->m_currentRobotAnimation.c_str(), sizeof(packet.robotAnim) - 1);
        packet.robotAnim[sizeof(packet.robotAnim) - 1] = '\0';
    }

    auto plr2 = editorLayer->m_player2;
    packet.hasSecond = plr2 && plr2->getParent() != nullptr;
    if (packet.hasSecond) {
        packet.x2 = plr2->getPositionX();
        packet.y2 = plr2->getPositionY();
        packet.rotation2 = plr2->getRotation();
        packet.playerScale2 = plr2->m_vehicleSize > 0.01f ? plr2->m_vehicleSize : 1.0f;
        packet.isUpsideDown2 = plr2->m_isUpsideDown;
        packet.isGoingLeft2 = plr2->m_isGoingLeft;
        packet.isDead2 = plr2->m_isDead;
        packet.gameMode2 = static_cast<uint8_t>(getLocalPlayerGameMode(plr2));
        packet.robotAnim2[0] = '\0';
        if (plr2->m_isRobot && !plr2->m_currentRobotAnimation.empty()) {
            strncpy(packet.robotAnim2, plr2->m_currentRobotAnimation.c_str(), sizeof(packet.robotAnim2) - 1);
            packet.robotAnim2[sizeof(packet.robotAnim2) - 1] = '\0';
        }
    }

    packet.iconData.iconID = gameManager->getPlayerFrame();
    packet.iconData.shipID = gameManager->getPlayerShip();
    packet.iconData.ballID = gameManager->getPlayerBall();
    packet.iconData.ufoID = gameManager->getPlayerBird();
    packet.iconData.waveID = gameManager->getPlayerDart();
    packet.iconData.robotID = gameManager->getPlayerRobot();
    packet.iconData.spiderID = gameManager->getPlayerSpider();
    packet.iconData.swingID = gameManager->getPlayerSwing();
    packet.iconData.jetpackID = gameManager->getPlayerJetpack();
    
    packet.iconData.color1ID = gameManager->getPlayerColor();
    packet.iconData.color2ID = gameManager->getPlayerColor2();
    packet.iconData.glowColor = gameManager->getPlayerGlowColor();
    packet.iconData.hasGlow = gameManager->getPlayerGlow();

    g_network->sendPacket(&packet, sizeof(packet));
}

void SyncManager::onRemotePlayerPosition(const PlayerPositionPacket& packet, LevelEditorLayer* editorLayer) {
    if (!editorLayer) return;
    
    uint32_t userId = packet.header.senderID;

    auto it = m_remotePlayers.find(userId);
    
    bool stopPlaytest = packet.stopPlaytest;

    if (stopPlaytest){
        if (it != m_remotePlayers.end()){
            auto remotePlayer = it->second.player;
            if (remotePlayer){
                remotePlayer->setVisible(false);
                remotePlayer->destroyObject();
            }
            auto remotePlayer2 = it->second.player2;
            if (remotePlayer2){
                remotePlayer2->setVisible(false);
                remotePlayer2->destroyObject();
            }
            m_remotePlayers.erase(it);
        }
        return;
    }

    if (it == m_remotePlayers.end()) {
        RemotePlayer rp;
        rp.player = createRemotePlayerVisual(editorLayer, packet.iconData, false);
        if (!rp.player) {
            log::error("Failed to create remote player!");
            return;
        }
        rp.appliedGameMode = static_cast<uint8_t>(PlayerGameMode::Cube);
        editorLayer->m_objectLayer->addChild(rp.player);
        m_remotePlayers[userId] = rp;
        
        log::info("Created remote player for user: {}", userId);
    }

    auto& rp = m_remotePlayers[userId];

    updateRemotePlayerVisual(
        rp.player,
        rp.appliedGameMode,
        rp.appliedIconFrame,
        static_cast<PlayerGameMode>(packet.gameMode),
        packet.iconData,
        packet.x,
        packet.y,
        packet.rotation,
        packet.playerScale,
        packet.isUpsideDown,
        packet.isGoingLeft,
        packet.isDead,
        safeStr(packet.robotAnim),
        rp.appliedRobotAnim
    );

    if (packet.hasSecond) {
        if (!rp.player2) {
            rp.player2 = createRemotePlayerVisual(editorLayer, packet.iconData, true);
            if (rp.player2) {
                rp.appliedGameMode2 = static_cast<uint8_t>(PlayerGameMode::Cube);
                editorLayer->m_objectLayer->addChild(rp.player2);
            }
        }
        if (rp.player2) {
            updateRemotePlayerVisual(
                rp.player2,
                rp.appliedGameMode2,
                rp.appliedIconFrame2,
                static_cast<PlayerGameMode>(packet.gameMode2),
                packet.iconData,
                packet.x2,
                packet.y2,
                packet.rotation2,
                packet.playerScale2,
                packet.isUpsideDown2,
                packet.isGoingLeft2,
                packet.isDead2,
                safeStr(packet.robotAnim2),
                rp.appliedRobotAnim2
            );
        }
    } else if (rp.player2) {
        rp.player2->setVisible(false);
        rp.player2->destroyObject();
        rp.player2 = nullptr;
        rp.appliedGameMode2 = 255;
        rp.appliedIconFrame2 = -1;
        rp.appliedRobotAnim2.clear();
    }
}

void SyncManager::cleanUpPlayers() {
    for (auto& [userId, remotePlayer] : m_remotePlayers) {
        if (remotePlayer.player) {
            if (remotePlayer.player->getParent()){
                remotePlayer.player->removeFromParent();
            }
        }
        if (remotePlayer.player2) {
            if (remotePlayer.player2->getParent()){
                remotePlayer.player2->removeFromParent();
            }
        }
    }
    m_remotePlayers.clear();
    m_lastPlayerSendTime = 0.0f;

    for (auto& [userId, cursor] : m_remoteCursors) {
        if (cursor && cursor->getParent()){
            cursor->removeFromParent();
        }
    }
    m_remoteCursors.clear();
}

void SyncManager::clearAllRemoteState(){
    for (auto& [userId, highlights] : m_remoteSelectionHighlights) {
        for (auto sprite : highlights){
            if (sprite){
                MouseTooltip::get()->unregisterRegion(sprite->getParent());
                sprite->removeFromParent();
            }
        }
    }
    m_remoteSelectionHighlights.clear();
    m_remoteSelections.clear();

    for (auto& [userId, cursor] : m_remoteCursors) {
        if (cursor && cursor->getParent()){
            cursor->removeFromParent();
        }
    }
    m_remoteCursors.clear();

    m_syncedObjects.clear();
    m_localObjects.clear();
    m_incomingChunks.clear();
    m_pendingObjectUpdates.clear();

    MouseTooltip::get()->clear();
}

void SyncManager::clearPeerState(uint32_t peerID) {
    // remove cursor
    auto cursorIt = m_remoteCursors.find(peerID);
    if (cursorIt != m_remoteCursors.end()) {
        if (cursorIt->second && cursorIt->second->getParent()) {
            cursorIt->second->removeFromParent();
        }
        m_remoteCursors.erase(cursorIt);
    }

    // remove selection highlights
    auto highlightsIt = m_remoteSelectionHighlights.find(peerID);
    if (highlightsIt != m_remoteSelectionHighlights.end()) {
        for (auto sprite : highlightsIt->second) {
            if (sprite) {
                MouseTooltip::get()->unregisterRegion(sprite->getParent());
                sprite->removeFromParent();
            }
        }
        m_remoteSelectionHighlights.erase(highlightsIt);
    }

    // remove selection data
    m_remoteSelections.erase(peerID);

    // remove remote player
    auto playerIt = m_remotePlayers.find(peerID);
    if (playerIt != m_remotePlayers.end()) {
        if (playerIt->second.player) {
            if (playerIt->second.player->getParent()) {
                playerIt->second.player->removeFromParent();
            }
            playerIt->second.player->destroyObject();
        }
        if (playerIt->second.player2) {
            if (playerIt->second.player2->getParent()) {
                playerIt->second.player2->removeFromParent();
            }
            playerIt->second.player2->destroyObject();
        }
        m_remotePlayers.erase(playerIt);
    }
}

GJEffectManager* SyncManager::getActiveEffectManager(){
    if (auto pl = PlayLayer::get()) return pl->m_effectManager;
    if (auto lel = LevelEditorLayer::get()) return lel->m_effectManager;
    return nullptr;
}

void SyncManager::restoreColor(SavedColorData ColorData) {
    auto mgr = getActiveEffectManager();
    if (!mgr) return;

    auto action = ColorAction::create({ColorData.r, ColorData.g, ColorData.b}, ColorData.blending, -1);
    action->m_colorID = ColorData.colorID;
    action->m_currentOpacity = ColorData.opacity;
    action->m_copyID= ColorData.copyID;

    m_applyingRemoteChanges = true;
    mgr->setColorAction(action, ColorData.colorID);
    m_applyingRemoteChanges = false;
}

std::unordered_map<int, ccColor3B> SyncManager::getAllChannelColors() {
    std::unordered_map<int, ccColor3B> result;
    auto mgr = getActiveEffectManager();
    if (!mgr) return result;

    auto actions = mgr->getAllColorActions();
    if (!actions) return result;

    for (auto action : CCArrayExt<ColorAction*>(actions)) {
        result[action->m_colorID] = action->m_fromColor;
    }

    return result;
}

void SyncManager::syncColorAction(ColorAction* action){
    auto newColor = action->m_fromColor;

    SavedColorData data;
    data.colorID = action->m_colorID;
    data.r = newColor.r;
    data.g = newColor.g;
    data.b = newColor.b;
    data.blending = action->m_blending ? 1 : 0;
    data.opacity = action->m_currentOpacity;
    data.copyID = action->m_copyID;

    ColorChannelsPacket packet{};
    packet.header.type = PacketType::COLOR_SYNC;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    packet.count = 1;
    packet.colorDat[0] = data;
    
    size_t sendSize = offsetof(ColorChannelsPacket, colorDat) + packet.count * sizeof(SavedColorData);
    g_network->sendPacket(&packet, sendSize);
}

void SyncManager::sendAllColors(uint32_t targetPeerID) {
    auto mgr = getActiveEffectManager();
    if (!mgr) return;
    auto actions = mgr->getAllColorActions();
    if (!actions || actions->count() == 0) return;

    ColorChannelsPacket packet{};
    packet.header.type = PacketType::COLOR_SYNC;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();

    size_t count = 0;
    for (auto action : CCArrayExt<ColorAction*>(actions)) {
        if (count >= 1000 || !action) break;
        packet.colorDat[count].colorID = action->m_colorID;
        packet.colorDat[count].r = action->m_fromColor.r;
        packet.colorDat[count].g = action->m_fromColor.g;
        packet.colorDat[count].b = action->m_fromColor.b;
        packet.colorDat[count].blending = action->m_blending ? 1 : 0;
        packet.colorDat[count].opacity = action->m_currentOpacity;
        packet.colorDat[count].copyID = action->m_copyID;
        count++;
    }
    packet.count = count;

    size_t sendSize = offsetof(ColorChannelsPacket, colorDat) + packet.count * sizeof(SavedColorData);

    if (targetPeerID != 0) {
        g_network->sendPacketToPeer(targetPeerID, &packet, sendSize);
    } else {
        g_network->sendPacket(&packet, sendSize);
    }
}