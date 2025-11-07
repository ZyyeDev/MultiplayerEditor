#include <Geode/Geode.hpp>

#include "JoinPopup.hpp"
#include "../network/NetworkManager.hpp"

extern NetworkManager* g_network;

extern bool g_isHost;
extern bool g_isInSession;

JoinPopup* JoinPopup::create(){
    auto ret = new JoinPopup();
    if (ret->initAnchored(320.0f,280.0f)){
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool JoinPopup::setup(){
    this->setTitle("Join Session");

    auto winSize = this->m_mainLayer->getContentSize();

    // IP input
    auto ipLabel = CCLabelBMFont::create("Host IP:","bigFont.fnt");
    ipLabel->setScale(0.5f);
    ipLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 30
    ));
    this->m_mainLayer->addChild(ipLabel);

    m_ipInput = TextInput::create(200.0f, "127.0.0.1", "chatFont.fnt");
    m_ipInput->setFilter("1234567890.");
    m_ipInput->setPosition(ccp(
        winSize.width/2,
        winSize.height/2
    ));
    this->m_mainLayer->addChild(m_ipInput);

    // Join button
    auto joinBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Join","goldFont.fnt","GJ_button_01.png",0.8f),
        this,
        menu_selector(JoinPopup::OnJoin)
    );
    joinBtn->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 -40
    ));

    auto menu = CCMenu::create();
    menu->addChild(joinBtn);
    menu->setPosition(0,0);
    this->m_mainLayer->addChild(menu);

    return true;
}

void JoinPopup::OnJoin(CCObject*){
    std::string ip = m_ipInput->getString();

    if (ip.empty()){
        FLAlertLayer::create("Error","Please enter a valid IP Adress!!", "OK")->show();
        return;
    }

    log::info("Attempting to join: {}", ip);

    // connect to ip
    if (g_network->connect(ip,7777)){
        g_isHost = false;
        g_isInSession = true;

        this->onClose(nullptr);

        // go to editor
        auto editorLayer = LevelEditorLayer::get();
        if (!editorLayer){
            auto level = GJGameLevel::create();
            level->m_levelName = "Collab Session";

            auto scene = CCScene::create();
            auto editorLayer = LevelEditorLayer::create(level, false);
            scene->addChild(editorLayer);

            CCDirector::sharedDirector()->replaceScene(CCTransitionScene::create(0.5f,scene));
        }
    } else {
        FLAlertLayer::create("Connection Failed", "Couldn't connect to host!", "OK")->show();
    }
}