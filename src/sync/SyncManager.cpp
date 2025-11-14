#include <Geode/Geode.hpp>
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

// TODO: Add more properties
ObjectData SyncManager::extractObjectData(GameObject* obj){
    ObjectData data;
    memset(&data, 0, sizeof(data));

    std::string uid = getObjectUid(obj);
    strncpy(data.uid, uid.c_str(), 31);

    // basic properties
    data.objectID = obj->m_objectID;
    data.x = obj->getPositionX();
    data.y = obj->getPositionY();
    data.rotation = obj->getRotation();
    data.scaleX = obj->getScaleX();
    data.scaleY = obj->getScaleY();
    data.isFlippedX = obj->isFlipX();
    data.isFlippedY = obj->isFlipY();

    // layer stuff
    data.zLayer = obj->m_zLayer;
    data.zOrder = obj->getZOrder();
    data.editorLayer = obj->m_editorLayer;
    data.editorLayer2 = obj->m_editorLayer2;

    // color properties
    data.baseColorID = *obj->m_baseColor; // i think its this?
    data.detailColorID = obj->m_activeDetailColorID;
    data.dontEnter = obj->m_isDontEnter;
    data.dontFade = obj->m_isDontFade;

    // groups
    data.groups = obj->m_groups;

    // visibility
    data.isVisible = obj->isVisible();
    data.highDetail = obj->m_isHighDetail;

    // link and unique
    data.linkedGroup = obj->m_linkedGroup;
    data.uniqueID = obj->m_uniqueID;

    // Text objects
    if (auto* textObj = typeinfo_cast<TextGameObject*>(obj)) {
        strncpy(data.textString, textObj->m_text.c_str(), 255);
        data.textString[255] = '\0';
    }

    // Triggers - only sync properties that actually exist
    if (auto* effectObj = typeinfo_cast<EffectGameObject*>(obj)) {
        data.duration = effectObj->m_duration;
        data.targetGroupID = effectObj->m_targetGroupID;
        data.touchTriggered = effectObj->m_isTouchTriggered;
        data.spawnTriggered = effectObj->m_isSpawnTriggered;
        data.multiTrigger = effectObj->m_isMultiTriggered;
        
        // Color properties
        data.opacity = effectObj->m_opacity;
        
        // Move trigger properties
        data.moveX = effectObj->m_moveModX;
        data.moveY = effectObj->m_moveModY;
        data.easingType = effectObj->m_easingType;
        data.easingRate = effectObj->m_easingRate;
        data.lockToPlayerX = effectObj->m_lockToPlayerX;
        data.lockToPlayerY = effectObj->m_lockToPlayerY;
        
        // Rotate trigger
        data.degrees = effectObj->m_rotationDegrees;
        data.times360 = effectObj->m_times360;
        data.lockObjectRotation = effectObj->m_lockObjectRotation;
        
        // Follow trigger
        data.followXMod = effectObj->m_followXMod;
        data.followYMod = effectObj->m_followYMod;
        data.followYSpeed = effectObj->m_followYSpeed;
        data.followYDelay = effectObj->m_followYDelay;
        data.followYOffset = effectObj->m_followYOffset;
        data.followYMaxSpeed = effectObj->m_followYMaxSpeed;
        
        // Pulse trigger
        data.pulseMode = effectObj->m_pulseMode;
        data.fadeIn = effectObj->m_fadeInDuration;
        data.hold = effectObj->m_holdDuration;
        data.fadeOut = effectObj->m_fadeOutDuration;
        data.mainOnly = effectObj->m_pulseMainOnly;
        data.detailOnly = effectObj->m_pulseDetailOnly;
        data.exclusiveMode = effectObj->m_pulseExclusive;
        
        // Common trigger properties
        data.centerGroupID = effectObj->m_centerGroupID;
        data.shakeStrength = effectObj->m_shakeStrength;
        data.shakeInterval = effectObj->m_shakeInterval;
        data.animationID = effectObj->m_animationID;
        data.activateGroup = effectObj->m_activateGroup;
        data.itemID = effectObj->m_itemID;
        data.targetColor = effectObj->m_targetColor;
        
        // Trigger exit
        data.triggerOnExit = effectObj->m_triggerOnExit;
    }

    return data;
}

void SyncManager::applyObjectData(GameObject* obj, const ObjectData& data) {
    if (!obj) return;

    obj->setPositionX(data.x);
    obj->setPositionY(data.y);
    obj->setRotation(data.rotation);
    obj->setScaleX(data.scaleX);
    obj->setScaleY(data.scaleY);
    obj->setFlipX(data.isFlippedX);
    obj->setFlipY(data.isFlippedY);
    obj->m_zLayer = data.zLayer;
    obj->setZOrder(data.zOrder);
    obj->m_editorLayer = data.editorLayer;
    obj->m_editorLayer2 = data.editorLayer2;

    // groups
    obj->m_groups = data.groups;

    // color properties
    *obj->m_baseColor = data.baseColorID;
    obj->m_activeDetailColorID = data.detailColorID;
    obj->m_isDontEnter = data.dontEnter;
    obj->m_isDontFade = data.dontFade;

    // visibility
    obj->setVisible(data.isVisible);
    obj->m_isHighDetail = data.highDetail;

    // other
    obj->m_linkedGroup = data.linkedGroup;
    obj->m_uniqueID = data.uniqueID;
}

GameObject* SyncManager::createObjectFromData(const ObjectData& data){
    auto obj = GameObject::createWithKey(data.objectID);
    if (!obj) {
        log::error("GameObject::createWithKey failed for ID {}", data.objectID);
        return nullptr;
    }
    
    applyObjectData(obj, data);
    return obj;
}

void SyncManager::onLocalObjectAdded(GameObject*obj){
    std::string uid = generateUID();
    trackObject(uid, obj);

    ObjectAddPacket packet;
    packet.header.type = PacketType::OBJECT_ADD;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    packet.object = extractObjectData(obj);

    g_network->sendPacket(&packet, sizeof(packet));
}

void SyncManager::onLocalObjectDestroyed(GameObject* obj) {
    if (!isTrackedObject(obj)) return;
    
    std::string uid = getObjectUid(obj);
    
    ObjectDeletePacket packet;
    packet.header.type = PacketType::OBJECT_DELETE;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    strncpy(packet.uid, uid.c_str(), 31);
    
    g_network->sendPacket(&packet, sizeof(packet));
    untrackObject(uid);
}

void SyncManager::onLocalObjectModified(GameObject* obj) {
    if (!isTrackedObject(obj)) return;
    
    ObjectModifyPacket packet;
    packet.header.type = PacketType::OBJECT_UPDATE;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    packet.object = extractObjectData(obj);
    
    g_network->sendPacket(&packet, sizeof(packet));
}

void SyncManager::onRemoteObjectAdded(const ObjectAddPacket& packet) {
    auto editor = getEditorLayer();
    if (!editor) {
        log::error("No editor layer!");
        return;
    }
    
    m_applyingRemoteChanges = true;
    
    auto obj = editor->createObject(packet.object.objectID, {packet.object.x, packet.object.y}, false);
    if (!obj) {
        log::error("Failed to create object from data!");
        m_applyingRemoteChanges = false;
        return;
    }
    
    applyObjectData(obj, packet.object);
    
    m_applyingRemoteChanges = false;
    
    trackObject(packet.object.uid, obj);
}

void SyncManager::onRemoteObjectDestroyed(const ObjectDeletePacket& packet) {
    auto it = m_syncedObjects.find(packet.uid);
    if (it == m_syncedObjects.end()) {
        log::warn("Tried to delete nonexistent object: {}", packet.uid);
        return;
    }
    
    auto editor = getEditorLayer();
    if (!editor) {
        log::error("No editor layer for destroy!");
        return;
    }
    
    GameObject* obj = it->second;
    
    untrackObject(packet.uid);
    
    m_applyingRemoteChanges = true;
    editor->removeObject(obj, true);
    m_applyingRemoteChanges = false;
}

void SyncManager::onRemoteObjectModified(const ObjectModifyPacket& packet) {
    auto it = m_syncedObjects.find(packet.object.uid);
    if (it == m_syncedObjects.end()) {
        log::warn("Tried to modify nonexistent object: {}", packet.object.uid);
        return;
    }
    
    GameObject* obj = it->second;
    applyObjectData(obj, packet.object);
}

void SyncManager::sendFullState() {
    auto editor = getEditorLayer();
    if (!editor) return;
    
    auto allObjects = editor->m_objects;
    if (!allObjects) return;
    
    log::info("Sending full state: {} objects", allObjects->count());
    
    for (auto obj : CCArrayExt<GameObject*>(allObjects)) {
        if (!isTrackedObject(obj)){
            std::string uid = generateUID();
            trackObject(uid, obj);
        }
    }
    
    for (auto obj : CCArrayExt<GameObject*>(allObjects)){
        ObjectAddPacket packet;
        packet.header.type = PacketType::OBJECT_ADD;
        packet.header.timestamp = getCurrentTimestamp();
        packet.header.senderID = g_network->getPeerID();
        packet.object = extractObjectData(obj);

        g_network->sendPacket(&packet, sizeof(packet));
    }

    onLocalLevelSettingsChanged();
}

void SyncManager::handlePacket(const uint8_t* data, size_t size) {
    if (size < sizeof(PacketHeader)) return;
    
    const PacketHeader* header = reinterpret_cast<const PacketHeader*>(data);
    
    switch (header->type) {
        case PacketType::OBJECT_ADD: {
            const ObjectAddPacket* packet = reinterpret_cast<const ObjectAddPacket*>(data);
            onRemoteObjectAdded(*packet);
            break;
        }
        case PacketType::OBJECT_DELETE: {
            const ObjectDeletePacket* packet = reinterpret_cast<const ObjectDeletePacket*>(data);
            onRemoteObjectDestroyed(*packet);
            break;
        }
        case PacketType::OBJECT_UPDATE: {
            const ObjectModifyPacket* packet = reinterpret_cast<const ObjectModifyPacket*>(data);
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
            
            // if this is the first chunk, clear previous selection
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
            const LevelSettingsPacket* packet = reinterpret_cast<const LevelSettingsPacket*>(data);
            onRemoteLevelSettingsChanged(*packet);
            break;
        }
        case PacketType::PLAYER_POSITION: {
            const PlayerPositionPacket* packet = reinterpret_cast<const PlayerPositionPacket*>(data);
            
            auto editorLayer = getEditorLayer();
            if (editorLayer){
                log::info("remote plr pos");
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

void SyncManager::onRemoteCursorUpdate(const uint32_t& userID, int x, int y){
    auto it = m_remoteCursors.find(userID);

    CCPoint position = ccp(x, y);

    if (it == m_remoteCursors.end()){
        auto editor = getEditorLayer();
        if (!editor){
            return;
        }
        if (!editor->m_objectLayer){
            return;
        }

        auto cursor = CCSprite::create("cursor.png"_spr);
        if (!cursor) {
            log::error("failed to create cursor sprite!");
            return;
        }
        
        editor->m_objectLayer->addChild(cursor);
        cursor->setZOrder(INT_MAX);
        cursor->setPosition(position);
        
        // TODO
        /*
        auto label = CCLabelBMFont::create(username, "chatFont.fnt");
        label->setScale(0.5f);
        cursor->addChild(label);
        label->setPosition(
            ccp(
                cursor->getContentSize().width / 2,
                cursor->getContentSize().height + 5
             ));
        */
        m_remoteCursors[userID] = cursor;
        
        log::info("created cursor for user: {}", userID);
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
    if (m_remoteSelectionHighlights.empty()){
        return;
    }
    if (m_remoteSelectionHighlights.contains(userID)){
        log::warn("m_remoteSelectionHighlights does not contain {}", userID);
        return;
    }
    if (m_remoteSelections.contains(userID)){
        log::warn("m_remoteSelections does not contain {}", userID);
        return;
    }
    // remove old highlights
    auto& highlights = m_remoteSelectionHighlights[userID];
    for (auto sprite : highlights){
        if (sprite){
            sprite->removeFromParent();
        }
    }
    highlights.clear();

    auto editor = getEditorLayer();
    if (!editor) return;
    if (!editor->m_objectLayer) return;

    auto& selection = m_remoteSelections[userID];

    for (const std::string& uid : selection){
        auto it = m_syncedObjects.find(uid);
        if (it == m_syncedObjects.end()) continue;

        GameObject* obj = it->second;

        auto highlight = CCSprite::create("square02_001.png");
        if (!highlight){
            log::error("highlight does not exist??");
            continue;
        }

        // each user should have its own color
        // for now remote selections will be cyan
        highlight->setColor({0, 255, 255});
        highlight->setOpacity(128);

        CCSize objSize = obj->getContentSize();
        highlight->setScaleX(objSize.width / highlight->getContentSize().width);
        highlight->setScaleY(objSize.height / highlight->getContentSize().height);
        
        highlight->setPosition(obj->getPosition());
        highlight->setRotation(obj->getRotation());
        highlight->setZOrder(obj->getZOrder() + 1);
        
        editor->m_objectLayer->addChild(highlight);
        m_remoteSelectionHighlights[userID].push_back(highlight);
    }
}

LevelSettingsData SyncManager::extractLevelSettings(){
    LevelSettingsData data;
    memset(&data, 0, sizeof(data));

    auto editor = getEditorLayer();
    if (!editor) {
        log::error("cant get editor layer!!");
        return data;
    }

    auto level = editor->m_level;
    if (!level) {
        log::error("level is null!");
        return data;
    }

    // audio settings
    data.songID = level->m_audioTrack;
    data.customSongID = level->m_songID;

    auto settings = editor->m_levelSettings;
    if (!settings) {
        log::error("m_levelSettings is null!!");
        return data;
    }
    
    // color settings - using pointer access
    data.backgroundColorID = settings->m_backgroundIndex;
    data.groundColorID = settings->m_groundIndex;
    //data.lineColorID = settings->m_lineIndex;
    //data.objectColorID = settings->m_objectIndex;
    //data.color1ID = settings->m_color1Index;
    //data.color2ID = settings->m_color2Index;
    //data.color3ID = settings->m_color3Index;
    //data.color4ID = settings->m_color4Index;
    
    data.backgroundIndex = settings->m_backgroundIndex;
    data.groundIndex = settings->m_groundIndex;
    data.fontIndex = settings->m_fontIndex;

    // gameplay settings
    data.isPlatformer = level->isPlatformer();
    data.gamemode = settings->m_startMode;
    //data.miniMode = settings->m_isMini;
    //data.dualMode = settings->m_isDualMode;
    data.twoPlayerMode = level->m_twoPlayerMode;
    data.speed = settings->m_startSpeed;

    // guideline
    //data.guidelineSpacing = settings->m_guidelineSpacing;

    return data;
}

void SyncManager::applyLevelSettings(const LevelSettingsData& data) {
    auto editor = getEditorLayer();
    if (!editor) {
        log::error("cant get editor layer");
        return;
    }

    auto level = editor->m_level;
    if (!level) {
        log::error("level is null!!");
        return;
    }

    m_applyingRemoteChanges = true;

    level->m_audioTrack = data.songID;
    level->m_songID = data.customSongID;

    auto settings = editor->m_levelSettings;
    if (!settings) {
        log::error("m_levelSettings is null!");
        m_applyingRemoteChanges = false;
        return;
    }
    
    // color settings
    settings->m_backgroundIndex = data.backgroundColorID;
    settings->m_groundIndex = data.groundColorID;
    //settings->m_lineIndex = data.lineColorID;
    //settings->m_objectIndex = data.objectColorID;
    //settings->m_color1Index = data.color1ID;
    //settings->m_color2Index = data.color2ID;
    //settings->m_color3Index = data.color3ID;
    //settings->m_color4Index = data.color4ID;
    
    settings->m_backgroundIndex = data.backgroundIndex;
    settings->m_groundIndex = data.groundIndex;
    settings->m_fontIndex = data.fontIndex;

    // gameplay
    level->m_twoPlayerMode = data.twoPlayerMode;
    settings->m_startMode = data.gamemode;
    //settings->m_isMini = data.miniMode;
    //settings->m_isDualMode = data.dualMode;
    settings->m_startSpeed = data.speed;

    // guideline
    //settings->m_guidelineSpacing = data.guidelineSpacing;
    
    if (editor->m_editorUI){
        editor->m_editorUI->updateButtons();
    }
    
    if (level->m_songID != 0) {
        // custom song
        auto songInfo = MusicDownloadManager::sharedState()->getSongInfoObject(level->m_songID);
        if (songInfo) {
            FMODAudioEngine::sharedEngine()->playMusic(songInfo->m_songUrl, true, 1.0f, 1);
        }
    } else {
        // official song
        FMODAudioEngine::sharedEngine()->playMusic(
            fmt::format("song{}.mp3", level->m_audioTrack), 
            true, 
            1.0f, 
            1
        );
    }
    
    m_applyingRemoteChanges = false;
}

void SyncManager::onRemoteLevelSettingsChanged(const LevelSettingsPacket& packet) {
    applyLevelSettings(packet.settings);
}

void SyncManager::onLocalLevelSettingsChanged() {
    LevelSettingsPacket packet;
    packet.header.type = PacketType::LEVEL_SETTINGS;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    packet.settings = extractLevelSettings();
    
    g_network->sendPacket(&packet, sizeof(packet));
    log::info("sent level settings to remote!");
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

void SyncManager::updatePlayerSync(float dt, LevelEditorLayer* editorLayer){
    if (!editorLayer) return;
    
    auto plr = editorLayer->m_player1;
    if (!plr){
        log::error("plr not found!!!");
        return;
    }

    m_lastPlayerSendTime += dt;

    if (m_lastPlayerSendTime >= 0.05f){
        m_lastPlayerSendTime = 0.0f;
        sendPlayerPosition(editorLayer);
    }
    
    for (auto& [userId, remotePlr] : m_remotePlayers){
        if (remotePlr.player && !remotePlr.player->m_isDead){
            remotePlr.player->setVisible(true);
        }
    }
}

void SyncManager::sendPlayerPosition(LevelEditorLayer* editorLayer){
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
            remotePlayer.player->removeFromParent();
        }
    }
    m_remotePlayers.clear();
    m_lastPlayerSendTime = 0.0f;
}