#include <Geode/Geode.hpp>
#include <sstream>
#include "SyncManager.hpp"
#include "../network/NetworkManager.hpp"
#include "../network/Packets.hpp"

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

void SyncManager::onLocalObjectAdded(GameObject* obj) {
    std::string uid = generateUID();
    trackObject(uid, obj);
    m_localObjects.insert(obj);
    
    gd::string gdString = obj->getSaveString(nullptr);
    std::string objString = std::string(gdString);
    
    ObjectStringPacket packet;
    packet.header.type = PacketType::OBJECT_ADD;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    
    strncpy(packet.uid, uid.c_str(), 31);
    packet.uid[31] = '\0';
    
    packet.stringLength = std::min(objString.length(), sizeof(packet.objectString) - 1);
    strncpy(packet.objectString, objString.c_str(), packet.stringLength);
    packet.objectString[packet.stringLength] = '\0';
    
    g_network->sendPacket(&packet, sizeof(packet));
}


void SyncManager::onLocalObjectDestroyed(GameObject* obj) {
    if (!isTrackedObject(obj)) return;
    
    std::string uid = getObjectUid(obj);
    
    for (auto& [userId, highlights] : m_remoteSelectionHighlights) {
        highlights.erase(std::remove_if(highlights.begin(), highlights.end(), [obj](CCSprite* sprite){
            return sprite && sprite->getParent() == obj;
        }), highlights.end());
    }

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
    
    ObjectStringPacket packet;
    packet.header.type = PacketType::OBJECT_UPDATE;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    
    strncpy(packet.uid, uid.c_str(), 31);
    packet.uid[31] = '\0';
    
    packet.stringLength = std::min(objString.length(), sizeof(packet.objectString) - 1);
    strncpy(packet.objectString, objString.c_str(), packet.stringLength);
    packet.objectString[packet.stringLength] = '\0';
    
    g_network->sendPacket(&packet, sizeof(packet));
}

void SyncManager::onRemoteObjectAdded(const ObjectStringPacket& packet) {
    auto editor = getEditorLayer();
    if (!editor) {
        log::error("No editor layer!");
        return;
    }
    
    m_applyingRemoteChanges = true;
    
    std::string objString(packet.objectString, packet.stringLength);
    
    int countBefore = editor->m_objects ? editor->m_objects->count() : 0;
    
    editor->createObjectsFromString(objString, false, false);
    
    int countAfter = editor->m_objects ? editor->m_objects->count() : 0;
    if (countAfter > countBefore) {
        GameObject* newObj = static_cast<GameObject*>(
            editor->m_objects->objectAtIndex(countAfter - 1)
        );
        
        std::string uid(packet.uid);
        trackObject(uid, newObj);
        
        log::info("Created object: {}", uid);
    } else {
        log::error("Object creation failed!");
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

    // no crashes (i hope)
    if (m_localObjects.count(obj)) {
        if (editor->m_undoObjects) editor->m_undoObjects->removeAllObjects();
        if (editor->m_redoObjects) editor->m_redoObjects->removeAllObjects();
    }
    
    m_localObjects.erase(obj);
    untrackObject(packet.uid);
    
    m_applyingRemoteChanges = true;
    editor->removeObject(obj, false);
    m_applyingRemoteChanges = false;
}

void SyncManager::onRemoteObjectModified(const ObjectStringPacket& packet) {
    std::string uid(packet.uid);
    auto it = m_syncedObjects.find(uid);
    
    if (it == m_syncedObjects.end()) return;
    
    GameObject* oldObj = it->second;
    auto editor = getEditorLayer();
    if (!editor || !editor->m_objects) return;
    
    m_applyingRemoteChanges = true;

    for (auto& [userId, highlights] : m_remoteSelectionHighlights) {
        highlights.erase(std::remove_if(highlights.begin(), highlights.end(), [oldObj](CCSprite* sprite){
            return sprite && sprite->getParent() == oldObj;
        }), highlights.end());
    }

    m_localObjects.erase(oldObj);
    editor->removeObject(oldObj, false);
    untrackObject(uid);
    
    std::string objString(packet.objectString, packet.stringLength);
    int countBefore = editor->m_objects->count();
    editor->createObjectsFromString(objString, false, false);
    
    int countAfter = editor->m_objects->count();
    if (countAfter > countBefore) {
        GameObject* newObj = static_cast<GameObject*>(
            editor->m_objects->objectAtIndex(countAfter - 1)
        );
        trackObject(uid, newObj);
    } else {
        log::error("Object update failed for uid: {}", uid);
    }
    
    m_applyingRemoteChanges = false;
}

void SyncManager::sendFullState(uint32_t targetPeerID) {
    auto editor = getEditorLayer();
    if (!editor) return;

    auto allObjects = editor->m_objects;
    if (!allObjects) return;

    for (int i = 0; i < allObjects->count(); i++) {
        auto obj = static_cast<GameObject*>(allObjects->objectAtIndex(i));

        if (!isTrackedObject(obj)) {
            std::string uid = generateUID();
            trackObject(uid, obj);
        }

        gd::string gdString = obj->getSaveString(nullptr);
        std::string objString = std::string(gdString);

        ObjectStringPacket packet;
        packet.header.type = PacketType::OBJECT_ADD;
        packet.header.timestamp = getCurrentTimestamp();
        packet.header.senderID = g_network->getPeerID();

        std::string uid = getObjectUid(obj);
        strncpy(packet.uid, uid.c_str(), 31);
        packet.uid[31] = '\0';

        packet.stringLength = std::min(objString.length(), sizeof(packet.objectString) - 1);
        strncpy(packet.objectString, objString.c_str(), packet.stringLength);
        packet.objectString[packet.stringLength] = '\0';

        g_network->sendPacketToPeer(targetPeerID, &packet, sizeof(packet));
    }

    LevelSettingsPacket settingsPkt;
    settingsPkt.header.type = PacketType::LEVEL_SETTINGS;
    settingsPkt.header.timestamp = getCurrentTimestamp();
    settingsPkt.header.senderID = g_network->getPeerID();
    std::string settingsStr = extractSettingsString();
    settingsPkt.settingsLength = (uint32_t)std::min(settingsStr.size(), sizeof(settingsPkt.settingsString) - 1);
    memcpy(settingsPkt.settingsString, settingsStr.c_str(), settingsPkt.settingsLength);
    settingsPkt.settingsString[settingsPkt.settingsLength] = '\0';
    g_network->sendPacketToPeer(targetPeerID, &settingsPkt, sizeof(settingsPkt));

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
            const HandshakePacket* packet = reinterpret_cast<const HandshakePacket*>(data);
            // i think we want to do this in another way but it works for now
            std::string myVersion = Mod::get()->getVersion().toNonVString();
            if (myVersion != packet->version){
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
            g_network->addPeer(packet->header.senderID, packet->username);
            if (g_isHost){
                g_network->sendLobbyState(packet->header.senderID);
                g_network->broadcastPeerJoined(packet->header.senderID, packet->username);
            }
            break;
        }
        case PacketType::KICK_USER: {
            const KickPacket* packet = reinterpret_cast<const KickPacket*>(data);
            if (!g_isHost && packet->userToKick == g_network->getPeerID()){
                g_network->gotKicked(packet->kickReason);
                break;
            }
            g_network->removePeer(packet->userToKick);
            log::info("peer left {}", packet->userToKick);
            break;
        }
        case PacketType::PEER_JOINED: {
            const PeerJoinedPacket* packet = reinterpret_cast<const PeerJoinedPacket*>(data);
            g_network->addPeer(packet->peerID, packet->username);
            log::info("peer joined {} ({})", packet->peerID, packet->username);
            break;
        }
        case PacketType::PEER_LEFT: {
            const PeerLeftPacket* packet = reinterpret_cast<const PeerLeftPacket*>(data);
            g_network->removePeer(packet->peerID);
            log::info("peer left {}", packet->peerID);
            break;
        }
        case PacketType::LOBBY_SYNC: {
            const LobbySyncPacket* packet = reinterpret_cast<const LobbySyncPacket*>(data);

            g_network->m_peersInLobby.clear();

            for (uint32_t i = 0; i < packet->memberCount; i++){
                g_network->addPeer(
                    packet->members[i].peerID,
                    packet->members[i].username
                );
            }

            log::info("lobby synced: {} members", packet->memberCount);
            break;
        }
        case PacketType::FULL_SYNC_REQUEST: {
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
            const ObjectStringPacket* packet = reinterpret_cast<const ObjectStringPacket*>(data);
            onRemoteObjectAdded(*packet);
            break;
        }
        case PacketType::OBJECT_DELETE: {
            const ObjectDeletePacket* packet = reinterpret_cast<const ObjectDeletePacket*>(data);
            onRemoteObjectDestroyed(*packet);
            break;
        }
        case PacketType::OBJECT_UPDATE: {
            const ObjectStringPacket* packet = reinterpret_cast<const ObjectStringPacket*>(data);
            onRemoteObjectModified(*packet);
            break;
        }
        case PacketType::MOUSE_MOVE: {
            const MousePacket* packet = reinterpret_cast<const MousePacket*>(data);
            onRemoteCursorUpdate(packet->header.senderID, packet->x, packet->y);
            break;
        }
        case PacketType::SELECT_CHANGE: {
            const SelectPacket* packet = reinterpret_cast<const SelectPacket*>(data);
            
            std::vector<std::string> uids;
            for (uint32_t i = 0; i < packet->countInChunk; i++) {
                uids.push_back(std::string(packet->uids[i]));
            }
            
            // if this is the first chunk clear previous
            if (packet->chunkIndex == 0) {
                m_remoteSelections[packet->header.senderID].clear();
            }
            
            for (const auto& uid : uids) {
                m_remoteSelections[packet->header.senderID].push_back(uid);
            }
            
            // update highlights if no more packets coming
            if (!packet->hasMore) {
                onRemoteSelectionChanged(packet->header.senderID);
            }
            
            break;
        }
        case PacketType::LEVEL_SETTINGS: {
            if (size < sizeof(PacketHeader) + sizeof(uint32_t)) break;
            const LevelSettingsPacket* packet = reinterpret_cast<const LevelSettingsPacket*>(data);
            onRemoteLevelSettingsChanged(*packet);
            break;
        }
        case PacketType::PLAYER_POSITION: {
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
                sprite->removeFromParent();
            }
        }
        highlights.clear();
    }

    auto editor = getEditorLayer();
    if (!editor) return;
    if (!editor->m_objectLayer) return;

    auto& selection = m_remoteSelections[userID];

    for (const std::string& uid : selection){
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
    }
}

std::string SyncManager::extractSettingsString() {
    auto editor = getEditorLayer();
    if (!editor) return "";
    gd::string gs = editor->getLevelString();
    std::string s(gs);
    size_t sep = s.find(';');
    return sep != std::string::npos ? s.substr(0, sep) : s;
}

void SyncManager::applySettingsString(const char* str, uint32_t len) {
    if (len == 0) return;
    auto editor = getEditorLayer();
    if (!editor || !editor->m_levelSettings) return;

    std::string settingsStr(str, len);
    gd::vector<gd::string> values;
    gd::vector<void*> exists;

    std::string token;
    std::stringstream ss(settingsStr);
    while (std::getline(ss, token, ',')) {
        if (!token.empty()) {
            values.push_back(gd::string(token.c_str()));
        }
    }
    exists.resize(values.size(), reinterpret_cast<void*>(1));

    m_applyingRemoteChanges = true;
    //editor->m_levelSettings->customObjectSetup(values, exists);
    m_applyingRemoteChanges = false;

    if (editor->m_editorUI) {
        editor->m_editorUI->updateButtons();
    }
}

void SyncManager::onRemoteLevelSettingsChanged(const LevelSettingsPacket& packet) {
    if (g_isHost) return;
    applySettingsString(packet.settingsString, packet.settingsLength);
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
            selection.erase(std::remove(selection.begin(), selection.end(), uid), selection.end());
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