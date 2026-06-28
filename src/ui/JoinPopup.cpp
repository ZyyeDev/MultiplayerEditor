#include <Geode/Geode.hpp>

#include "../sync/SyncManager.hpp"
#include "JoinPopup.hpp"
#include "../network/NetworkManager.hpp"

extern NetworkManager* g_network;
extern SyncManager* g_sync;

extern bool g_isHost;
extern bool g_isInSession;

JoinPopup* JoinPopup::create(){
    auto ret = new JoinPopup();
    if (ret->init()){
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool JoinPopup::init(){
    if (!Popup::init(320.0f, 280.0f)) return false;
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

    // port input
    auto portLabel = CCLabelBMFont::create("Port:","bigFont.fnt");
    portLabel->setScale(0.5f);
    portLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 - 25
    ));
    this->m_mainLayer->addChild(portLabel);

    m_portInput = TextInput::create(100.0f, std::to_string(g_network->m_port), "chatFont.fnt");
    m_portInput->setFilter("1234567890");
    m_portInput->setString(std::to_string(g_network->m_port));
    m_portInput->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 - 55
    ));
    this->m_mainLayer->addChild(m_portInput);

    // Join button
    auto joinBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Join","goldFont.fnt","GJ_button_01.png",0.8f),
        this,
        menu_selector(JoinPopup::OnJoin)
    );
    joinBtn->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 -90
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

    uint16_t port = g_network->m_port;
    std::string portStr = m_portInput->getString();
    if (!portStr.empty()){
        try {
            port = static_cast<uint16_t>(std::stoi(portStr));
        } catch (...) {
            port = g_network->m_port;
        }
    }

    log::info("Attempting to join: {}:{}", ip, port);

    // connect to ip
    if (g_network->connect(ip,port)){
        g_isHost = false;
        g_isInSession = true;

        g_sync->setUserID(g_network->getPeerID());

        this->onClose(nullptr);

        // go to editor
        auto level = GJGameLevel::create();
        level->m_levelName = "Collab Session";

        auto scene = CCScene::create();
        auto editorLayer = LevelEditorLayer::create(level, false);
        scene->addChild(editorLayer);

        CCDirector::sharedDirector()->replaceScene(CCTransitionFade::create(0.5f,scene));
        
    } else {
        FLAlertLayer::create("Connection Failed", "Couldn't connect to host!", "OK")->show();
    }
}