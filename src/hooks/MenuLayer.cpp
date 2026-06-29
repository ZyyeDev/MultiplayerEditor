#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "../ui/JoinPopup.hpp"
#include "../utils/MouseTooltip.hpp"

using namespace geode::prelude;

class $modify(CollabMenuLayer, MenuLayer){
    
    struct Fields {
        CCMenuItemSpriteExtra* m_joinBtn = nullptr;
    };

    bool init(){
        if (!MenuLayer::init()) return false;

        auto bottomMenu = this->getChildByID("bottom-menu");
        if (!bottomMenu) return false;

        /* ---- CREATE BUTTON ---- */
        auto joinSprite = CCSprite::create("multiplayerCollabButton.png"_spr);
        
        auto joinBtn = CCMenuItemSpriteExtra::create(
            joinSprite,
            this,
            menu_selector(CollabMenuLayer::onJoinSession)
        );

        joinBtn->setPosition(ccp(-150,0));
        bottomMenu->addChild(joinBtn);
        bottomMenu->updateLayout();

        m_fields->m_joinBtn = joinBtn;

        MouseTooltip::get()->registerRegion(joinBtn, "Join A Lobby By IP", {80, 100, 255});

        /* --------------------- */

        return true;
    }

    void onExit(){
        MenuLayer::onExit();
        
        if (m_fields->m_joinBtn) {
            MouseTooltip::get()->unregisterRegion(m_fields->m_joinBtn);
        }
    }

    void onJoinSession(CCObject*){
        JoinPopup::create()->show();
    }
};