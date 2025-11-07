#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include "../ui/JoinPopup.hpp"

using namespace geode::prelude;

class $modify(CollabMenuLayer, MenuLayer){
    bool init(){
        if (!MenuLayer::init()) return false;

        auto bottomMenu = this->getChildByID("bottom-menu");
        if (!bottomMenu) return false;

        /* ---- CREATE BUTTON ---- */
        // TODO: Change sprite to a custom one, not just a normal button with text
        auto joinSprite = ButtonSprite::create(
            "Join Session",
            "goldFont.fnt",
            "GJ_button_01.png",
            0.8f
        );
        
        auto joinBtn = CCMenuItemSpriteExtra::create(
            joinSprite,
            this,
            menu_selector(CollabMenuLayer::onJoinSession)
        );

        joinBtn->setPosition(ccp(-150,0));
        bottomMenu->addChild(joinBtn);
        bottomMenu->updateLayout();
        /* --------------------- */

        return true;
    }

    void onJoinSession(CCObject*){
        JoinPopup::create()->show();
    }
};