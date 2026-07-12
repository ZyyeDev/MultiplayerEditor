#include <Geode/Geode.hpp>
#include <sstream>
#include <cstring>
#include "SyncManager.hpp"
#include "../network/NetworkManager.hpp"
#include "../network/Packets.hpp"
#include "../utils/MouseTooltip.hpp"

extern NetworkManager* g_network;
extern bool g_isHost;

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

        g_network->sendPacket(&packet, sizeof(packet));

        offset += thisChunkLen;
        chunkIndex++;

        if (!hasMore) break;
    }
}

void SyncManager::onLocalObjectAdded(GameObject* obj) {
    std::string uid = generateUID();
    trackObject(uid, obj);
    m_localObjects.insert(obj);
    
    gd::string gdString = obj->getSaveString(nullptr);
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
    
    gd::string gdString = obj->getSaveString(nullptr);
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
    
    int countBefore = editor->m_objects ? editor->m_objects->count() : 0;
    
    editor->createObjectsFromString(objString, false, false);
    
    int countAfter = editor->m_objects ? editor->m_objects->count() : 0;
    if (countAfter > countBefore) {
        GameObject* newObj = static_cast<GameObject*>(
            editor->m_objects->objectAtIndex(countAfter - 1)
        );
        
        trackObject(uid, newObj);
        
        log::info("created object: {}", uid);
    } else {
        log::error("object creation failed!");
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

    // no crashes (i hope)
    if (m_localObjects.count(obj)) {
        if (editor->m_undoObjects) editor->m_undoObjects->removeAllObjects();
        if (editor->m_redoObjects) editor->m_redoObjects->removeAllObjects();
    }
    
    m_localObjects.erase(obj);
    untrackObject(packet.uid);
    
    m_applyingRemoteChanges = true;
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
    if (editor->m_objects->containsObject(oldObj)) {
        editor->removeObject(oldObj, false);
    }
    untrackObject(uid);
    
    int countBefore = editor->m_objects->count();
    editor->createObjectsFromString(objString, false, false);
    
    int countAfter = editor->m_objects->count();
    if (countAfter > countBefore) {
        GameObject* newObj = static_cast<GameObject*>(
            editor->m_objects->objectAtIndex(countAfter - 1)
        );
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

        gd::string gdString = obj->getSaveString(nullptr);
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

            g_network->sendPacketToPeer(targetPeerID, &pkt, sizeof(pkt));

            offset += thisChunkLen;
            chunkIndex++;
            if (!hasMore) break;
        }

    }

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
    
    switch (header->type) {
        case PacketType::HANDSHAKE: {
            if (size < sizeof(HandshakePacket)) break;
            const HandshakePacket* packet = reinterpret_cast<const HandshakePacket*>(data);
            // i think we want to do this in another way but it works for now
            std::string myVersion = Mod::get()->getVersion().toNonVString();
            log::info("got handshake from {}, their version '{}' my version '{}'", packet->header.senderID, packet->version, myVersion);
            
            log::info("registering peer {} as '{}'", packet->header.senderID, packet->username);
            g_network->addPeer(packet->header.senderID, packet->username);
            if (g_isHost){
                g_network->broadcastPeerJoined(packet->header.senderID, packet->username);

                // we MUST do this in another way
                if (!g_network->checkPassword(packet->password)){
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

                if (myVersion != packet->version){
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
                g_network->gotKicked(packet->kickReason);
                break;
            }
            g_network->removePeer(packet->userToKick);
            m_remoteSelections[packet->userToKick].clear();
            onRemoteSelectionChanged(packet->userToKick);
            m_remoteSelections.erase(packet->userToKick);
            m_remoteSelectionHighlights.erase(packet->userToKick);
            log::info("peer left {}", packet->userToKick);
            break;
        }
        case PacketType::PEER_JOINED: {
            if (size < sizeof(PeerJoinedPacket)) break;
            const PeerJoinedPacket* packet = reinterpret_cast<const PeerJoinedPacket*>(data);
            g_network->addPeer(packet->peerID, packet->username);
            log::info("peer joined {} ({})", packet->peerID, packet->username);
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
            m_syncedObjects.clear();
            m_objectToUID.clear();
            trackExistingObjects();
            auto editor = getEditorLayer();
            if (editor && editor->m_editorUI) {
                editor->m_editorUI->updateButtons();
            }
            break;
        }
        case PacketType::OBJECT_ADD: {
            if (size < sizeof(ObjectStringPacket)) break;
            const ObjectStringPacket* packet = reinterpret_cast<const ObjectStringPacket*>(data);
            std::string uid(packet->uid);

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
            if (size < sizeof(ObjectStringPacket)) break;
            const ObjectStringPacket* packet = reinterpret_cast<const ObjectStringPacket*>(data);
            std::string uid(packet->uid);

            if (packet->chunkIndex == 0) m_incomingChunks[uid] = ChunkBuffer{};
            m_incomingChunks[uid].data.append(packet->objectString, packet->chunkLength);
            m_incomingChunks[uid].lastChunkIndex = packet->chunkIndex;

            if (!packet->hasMore) {
                std::string fullString = m_incomingChunks[uid].data;
                m_incomingChunks.erase(uid);
                onRemoteObjectModified(uid, fullString);
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
            if (packet->count > 200) break;
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
                std::string uid(packet->uids[i]);
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

void SyncManager::onRemoteLevelSettingsChanged(const LevelSettingsPacket& packet) {
    if (g_isHost) return;
    
    applyLevelSettings(packet);
}

void SyncManager::applyLevelSettings(const LevelSettingsPacket& settings) {
    auto editor = getEditorLayer();
    if (!editor) return;

    m_applyingRemoteChanges = true;

    std::string saveStr(settings.settingsString, settings.settingsLength);

    if (!saveStr.empty() && editor->m_levelSettings) {
        auto* newSettings = LevelSettingsObject::objectFromString(saveStr);
        if (newSettings) {
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
    packet.isUpsideDown = plr->m_isUpsideDown;
    packet.isDead = plr->m_isDead;
    packet.stopPlaytest = stopPlaytest;

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
            m_remotePlayers.erase(it);
        }
        return;
    }

    if (it == m_remotePlayers.end()) {
        auto remotePlayer = PlayerObject::create(
            packet.iconData.iconID,
            packet.iconData.shipID,
            editorLayer,
            editorLayer->m_objectLayer,
            false
        );

        if (!remotePlayer) {
            log::error("Failed to create remote player!");
            return;
        }
        
        remotePlayer->setOpacity(200);
        remotePlayer->setPosition(ccp(packet.x, packet.y));
        remotePlayer->setRotation(packet.rotation);
        remotePlayer->m_isUpsideDown = packet.isUpsideDown;
        remotePlayer->m_isDead = packet.isDead;
        
        auto gameManager = GameManager::sharedState();
        remotePlayer->setColor(gameManager->colorForIdx(packet.iconData.color1ID));
        remotePlayer->setSecondColor(gameManager->colorForIdx(packet.iconData.color2ID));
        remotePlayer->setZOrder(1000);
        
        if (packet.iconData.hasGlow) {
            remotePlayer->enableCustomGlowColor(gameManager->colorForIdx(packet.iconData.glowColor));
        } else {
            remotePlayer->disableCustomGlowColor();
        }
        
        editorLayer->m_objectLayer->addChild(remotePlayer);
        
        RemotePlayer rp;
        rp.player = remotePlayer;
        rp.userId = userId;
        m_remotePlayers[userId] = rp;
        
        log::info("Created remote player for user: {}", userId);
    } else {
        auto remotePlayer = it->second.player;
        if (!remotePlayer){
            log::error("remote player is null!");
            return;
        }

        remotePlayer->setPosition(ccp(packet.x, packet.y));
        remotePlayer->setRotation(packet.rotation);
        remotePlayer->m_isUpsideDown = packet.isUpsideDown;

        if (packet.isDead) {
            remotePlayer->m_isDead = true;
            remotePlayer->setVisible(false);
        } else {
            remotePlayer->m_isDead = false;
            remotePlayer->setVisible(true);
        }
    }
}

void SyncManager::cleanUpPlayers() {
    for (auto& [userId, remotePlayer] : m_remotePlayers) {
        if (remotePlayer.player) {
            if (remotePlayer.player->getParent()){
                remotePlayer.player->removeFromParent();
            }
        }
    }
    m_remotePlayers.clear();
    m_lastPlayerSendTime = 0.0f;
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

    MouseTooltip::get()->clear();
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