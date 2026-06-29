#include "MouseTooltip.hpp"

MouseTooltip* MouseTooltip::get(){
    static MouseTooltip instance;
    return &instance;
}

void MouseTooltip::registerRegion(CCNode* node, const std::string& text, ccColor3B color){
    if (!node){
        log::warn("are we deadass??");
        return;
    }

    log::info("registering tooltip region9");

    unregisterRegion(node);
    m_regions.push_back({node, text, color});
}

void MouseTooltip::unregisterRegion(CCNode* node){
    m_regions.erase(std::remove_if(m_regions.begin(), m_regions.end(), [node](const TooltipRegion& region){
        return region.node == node;
    }), m_regions.end());
}

void MouseTooltip::clear(){
    m_regions.clear();
    hideTooltip();
}

void MouseTooltip::ensureLabel(){
    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    if (m_bg && m_bg->getParent() == scene) return;

    if (m_bg){
        m_bg->removeFromParent();
        m_bg->release();
        m_bg = nullptr;
        m_label = nullptr;
    }

    m_bg = CCLayerColor::create({0, 0, 0, 180});
    m_bg->retain();
    m_bg->setContentSize({100, 24});
    m_bg->setZOrder(INT_MAX);
    m_bg->setVisible(false);
    scene->addChild(m_bg, INT_MAX);

    m_label = CCLabelBMFont::create("", "chatFont.fnt");
    m_label->setScale(0.6f);
    m_label->setAnchorPoint({0, 0});
    m_label->setPosition({4, 4});
    m_bg->addChild(m_label);
}

void MouseTooltip::showTooltip(const std::string& text, ccColor3B color, CCPoint pos){
    ensureLabel();
    if (!m_label || !m_bg) return;

    m_label->setString(text.c_str());
    m_label->setColor(color);

    auto labelSize = m_label->getContentSize();
    m_bg->setContentSize({labelSize.width + 8, labelSize.height + 8});
    m_bg->setPosition({pos.x + 8, pos.y + 8});
    m_bg->setVisible(true);
}

void MouseTooltip::hideTooltip(){
    if (m_bg && m_bg->getParent()){
        m_bg->setVisible(false);
    }
}

void MouseTooltip::update(){
    // WHY THE FUCK ARE YOU CRASHING GASFDGASDF
    // edit: nvm i fixed it
    m_regions.erase(
        std::remove_if(m_regions.begin(), m_regions.end(), [](const TooltipRegion& region) {
            auto locked = region.node.lock();
            return !locked || !locked->getParent();
        }), 
        m_regions.end()
    );

    if (m_regions.empty()){
        hideTooltip();
        return;
    }

    CCPoint mousePos = geode::cocos::getMousePos();

    for (auto& region : m_regions) {
        auto node = region.node.lock();
        if (!node || !node->getParent()) continue;

        CCRect box = node->boundingBox();
        auto parent = node->getParent();

        CCPoint bottomLeft = parent->convertToWorldSpace(box.origin);
        CCPoint topRight = parent->convertToWorldSpace(ccp(box.origin.x + box.size.width, box.origin.y + box.size.height));

        bool inside = mousePos.x >= bottomLeft.x && mousePos.x <= topRight.x &&
                      mousePos.y >= bottomLeft.y && mousePos.y <= topRight.y;

        if (inside){
            showTooltip(region.text, region.color, mousePos);
            return;
        }
    }

    hideTooltip();
}