#include <Geode/Geode.hpp>
#include <Geode/modify/EditorPauseLayer.hpp>

#include "../network/NetworkManager.hpp"
#include "../ui/HostPopup.hpp"

using namespace geode::prelude;

extern NetworkManager* g_network;
extern bool g_isInSession;
extern bool g_isHost;

class $modify(CollabEditorPauseLayer, EditorPauseLayer){
    bool init(LevelEditorLayer* lel){
        if (!EditorPauseLayer::init(lel)) return false;

        // we dont want to show the button to clients
        if (g_isInSession && !g_isHost){
            return true;
        }

        auto menu = this->getChildByType<CCMenu>(0);
        if (!menu){
            log::error("cant find EditorPauseLayer!");
            return true;
        }

        auto hostSprite = ButtonSprite::create(
            "Host Session",
            "goldFont.fnt",
            "GJ_button_01.png",
            1.0f
        );
        //hostSprite->setScale(.7f);

        auto hostBtn = CCMenuItemSpriteExtra::create(
            hostSprite,
            this,
            menu_selector(CollabEditorPauseLayer::onHostSession)
        );

        menu->addChild(hostBtn);
        menu->updateLayout();

        return true;
    }

    void onHostSession(CCObject*) {
        //if (g_isInSession) {
        //    FLAlertLayer::create("Already in session", "You're already hosting or in a session!", "OK")->show();
        //    return;
        //}
        
        HostPopup::create()->show();
    }
};
