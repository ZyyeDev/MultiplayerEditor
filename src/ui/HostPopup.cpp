#include "HostPopup.hpp"
#include "../network/NetworkManager.hpp"
#include "../sync/SyncManager.hpp"

extern NetworkManager* g_network;
extern SyncManager* g_sync;

extern bool g_isHost;
extern bool g_isInSession;

HostPopup* HostPopup::create(){
    auto ret = new HostPopup();
    if (ret->init()){
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool HostPopup::init(){
    if (!Popup::init(320.0f, 280.0f)) return false;
    this->setTitle("Host Session");

    auto winSize = this->m_mainLayer->getContentSize();

    // info text
    auto infoLabel = CCLabelBMFont::create("Host IP:", "bigFont.fnt");
    infoLabel->setScale(.4f);
    infoLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 20
    ));
    this->m_mainLayer->addChild(infoLabel);

    // ip label
    m_ipLabel = CCLabelBMFont::create("(Host To Show IP)","chatFont.fnt");
    m_ipLabel->setScale(.6f);
    m_ipLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 65
    ));
    this->m_mainLayer->addChild(m_ipLabel);

    // help label
    m_helpLabel = CCLabelBMFont::create("Give your hamachi or public ip, NOT THE LOCAL ONE (above)","goldFont.fnt");
    m_helpLabel->setScale(.4f);
    m_helpLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 45
    ));
    m_helpLabel->setVisible(false);
    this->m_mainLayer->addChild(m_helpLabel);
    
    if (g_isHost && g_isInSession){
        std::string ipStr = "127.0.0.1:" + std::to_string(g_network->m_port);
        m_ipLabel->setString(ipStr.c_str());
        m_helpLabel->setVisible(true);
    }

    // port input
    m_portInput = TextInput::create(100.0f, std::to_string(g_network->m_port), "chatFont.fnt");
    m_portInput->setFilter("1234567890");
    m_portInput->setString(std::to_string(g_network->m_port));
    m_portInput->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 - 10
    ));
    this->m_mainLayer->addChild(m_portInput);

    // host button
    hostBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Host", "goldFont.fnt", "GJ_button_01.png", .8f),
        this,
        menu_selector(HostPopup::onStartHost)
    );
    hostBtn->setPosition(ccp(
        winSize.width/2,
        winSize.height/2-40
    ));
    if (g_isHost && g_isInSession){
        hostBtn->setSprite(ButtonSprite::create("Stop Hosting", "goldFont.fnt", "GJ_button_01.png", .8f));
    }

    // popup menu
    auto menu = CCMenu::create();
    menu->addChild(hostBtn);
    menu->setPosition(0, 0);
    this->m_mainLayer->addChild(menu);

    return true;
}

void HostPopup::onStartHost(CCObject*){
    if (g_isHost && g_isInSession){
        g_network->stopHosting();
        g_isHost = false;
        g_isInSession = false;

        if (g_sync){
            g_sync->cleanUpPlayers();
            g_sync->clearAllRemoteState();
        }

        if (hostBtn){
            hostBtn->setSprite(ButtonSprite::create("Host", "goldFont.fnt", "GJ_button_01.png", .8f));
        }

        FLAlertLayer::create(
            "Stopped Hosting!",
            "You are no longer hosting!",
            "OK"
        )->show();
        return;
    }

    uint16_t port = g_network->m_port;
    if (m_portInput){
        std::string portStr = m_portInput->getString();
        if (!portStr.empty()){
            try {
                port = static_cast<uint16_t>(std::stoi(portStr));
            } catch (...) {
                port = g_network->m_port;
            }
        }
    }

    if (g_network->host(port)){
        if (hostBtn){
            hostBtn->setSprite(ButtonSprite::create("Stop Hosting", "goldFont.fnt", "GJ_button_01.png", .8f));
        }
        g_isHost = true;
        g_isInSession = true;

        g_sync->setUserID(g_network->getPeerID());

        // for now, just show local ip
        // it should show local ip, or hamachi ip (or whatever the user is using)
        std::string ipStr = "127.0.0.1:" + std::to_string(port);
        m_ipLabel->setString(ipStr.c_str());
        m_helpLabel->setVisible(true);

        g_sync->trackExistingObjects();
        
        log::info("it works!!! :3");

        FLAlertLayer::create(
            "Hosting!",
            "You are now hosting!",
            "OK"
        )->show();
    } else {
        FLAlertLayer::create(
            "Error",
            "Failed to host!",
            "OK"
        )->show();
    }
}