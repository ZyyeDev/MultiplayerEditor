#include <Geode/Geode.hpp>
#include "SyncManager.hpp"
#include "../network/NetworkManager.hpp"
#include "../network/Packets.hpp"

extern NetworkManager* g_network;
extern bool g_isHost;

SyncManager::SyncManager() : m_objectCounter(0), m_lastUpdateTimestamp(0) {
    if (g_isHost){
        m_userID = "host";
    }else{
        uint32_t peerID = g_network->getPeerID();
        m_userID = "peer_" + std::to_string(peerID);
    }

    g_network->setOnRecive([this](const uint8_t* data, size_t size){
        this->handlePacket(data, size);
    });
}

std::string SyncManager::generateUID() {
    return m_userID + "_" + std::to_string(m_objectCounter++);
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
    data.baseColorID = obj->m_activeMainColorID; // i think its this?
    data.detailColorID = obj->m_activeDetailColorID;
    data.dontEnter = obj->m_isDontEnter;
    data.dontFade = obj->m_isDontFade;

    // groups TODO
    //data.groups = obj->m_groups;

    // visibility
    data.isVisible = obj->isVisible();
    data.highDetail = obj->m_isHighDetail;

    // Triggers
    if (auto* effectObj = typeinfo_cast<EffectGameObject*>(obj)) {
        data.duration = effectObj->m_duration;
        data.targetGroupID = effectObj->m_targetGroupID;
        data.touchTriggered = effectObj->m_isTouchTriggered;
        data.spawnTriggered = effectObj->m_isSpawnTriggered;
        data.multiTrigger = effectObj->m_isMultiTriggered;
        
        // Color trigger properties
        if (effectObj->m_targetColor) {
            data.colorID = effectObj->m_targetColor;
            data.red = effectObj->getColor().r / 255.0f;
            data.green = effectObj->getColor().g / 255.0f;
            data.blue = effectObj->getColor().b / 255.0f;
            data.opacity = effectObj->m_opacity;
            data.fadeTime = effectObj->m_fadeInDuration;
            data.blending = effectObj->m_usesBlending;
        }
        
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
        
        // Pulse trigger
        data.pulseMode = effectObj->m_pulseMode;
        data.pulseType = effectObj->m_pulseTargetType;
        data.fadeIn = effectObj->m_fadeInDuration;
        data.hold = effectObj->m_holdDuration;
        data.fadeOut = effectObj->m_fadeOutDuration;
        data.mainOnly = effectObj->m_pulseMainOnly;
        data.detailOnly = effectObj->m_pulseDetailOnly;
        data.exclusiveMode = effectObj->m_pulseExclusive;
        
        // Spawn trigger
        data.delayTime = effectObj->m_spawnTriggerDelay;
        
        // Collision
        data.activateOnExit = effectObj->m_triggerOnExit;
        // TODO: Find these
        //data.blockAID = effectObj->;
        //data.blockBID = effectObj->;
        
        // Alpha trigger
        data.targetOpacity = effectObj->m_opacity;
        
        // Item ID (for count triggers, pickup, etc)
        data.itemID = effectObj->m_itemID;
        data.targetCount = effectObj->m_itemID2; // i think its itemID2
        
        data.centerGroupID = effectObj->m_centerGroupID;
        data.shakeStrength = effectObj->m_shakeStrength;
        data.shakeInterval = effectObj->m_shakeInterval;
        data.animationID = effectObj->m_animationID;
        data.activateGroup = effectObj->m_activateGroup;
        /*
        TODO: Find these
        data.comparison = effectObj->m_comparison;
        data.holdMode = effectObj->m_holdMode;
        data.toggleMode = effectObj->m_toggleMode;
        data.multiActivate = effectObj->m_multiActivate;
        
        data.targetPosMode = effectObj->m_targetPosMode;
        data.targetPosXMod = effectObj->m_targetPosXMod;
        data.targetPosYMod = effectObj->m_targetPosYMod;
        data.dynamicMode = effectObj->m_dynamicMode;
        */
    }

    // other
    data.linkedGroup = obj->m_linkedGroup;
    data.uniqueID = obj->m_uniqueID;

    return data;
}

void SyncManager::applyObjectData(GameObject* obj, const ObjectData& data) {
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
    log::info("Set object add: {}",uid);
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
    
    log::info("Sent object delete: {}", uid);
}

void SyncManager::onLocalObjectModified(GameObject* obj) {
    if (!isTrackedObject(obj)) return;
    
    ObjectModifyPacket packet;
    packet.header.type = PacketType::OBJECT_UPDATE;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    packet.object = extractObjectData(obj);
    
    g_network->sendPacket(&packet, sizeof(packet));
    log::info("Sent object modify: {}", getObjectUid(obj));
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
    log::info("Added remote object: {}", packet.object.uid);
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
    
    log::info("Deleted object: {}", packet.uid);
}

void SyncManager::onRemoteObjectModified(const ObjectModifyPacket& packet) {
    auto it = m_syncedObjects.find(packet.object.uid);
    if (it == m_syncedObjects.end()) {
        log::warn("Tried to modify nonexistent object: {}", packet.object.uid);
        return;
    }
    
    GameObject* obj = it->second;
    applyObjectData(obj, packet.object);
    
    log::info("Modified remote object: {}", packet.object.uid);
}

void SyncManager::sendFullState() {
    auto editor = getEditorLayer();
    if (!editor) return;
    
    auto allObjects = editor->m_objects;
    if (!allObjects) return;
    
    log::info("Sending full state: {} objects", allObjects->count());
    
    for (auto obj : CCArrayExt<GameObject*>(allObjects)) {
        onLocalObjectAdded(obj);
    }
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
    m_CursorPos = position;
    
    MousePacket packet;
    packet.header.type = PacketType::MOUSE_MOVE;
    packet.header.timestamp = getCurrentTimestamp();
    packet.header.senderID = g_network->getPeerID();
    packet.x = position.x;
    packet.y = position.y;
    
    g_network->sendPacket(&packet, sizeof(packet));
}

void SyncManager::onRemoteCursorUpdate(const std::string& userID, int x, int y){
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
        editor->m_objectLayer->addChild(cursor, 9999);
        cursor->setPosition(position);

        m_remoteCursors[userID] = cursor;
    }else{
        it->second->setPosition(position);
    }
}