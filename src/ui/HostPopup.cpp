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

    // ip label
    m_ipLabel = CCLabelBMFont::create("(Start hosting to see your IP)","chatFont.fnt");
    m_ipLabel->setScale(.6f);
    m_ipLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 90
    ));
    this->m_mainLayer->addChild(m_ipLabel);

    // help label
    m_helpLabel = CCLabelBMFont::create("Same WiFi/network: others type the IP and PORT above.\nDifferent network: port-forward this port and share your public IP instead.\nUsing Hamachi/Playit/similar: share the address IT gives you, not this one.","goldFont.fnt");
    m_helpLabel->setScale(.4f);
    m_helpLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 75
    ));
    m_helpLabel->setVisible(false);
    this->m_mainLayer->addChild(m_helpLabel);
    
    if (g_isHost && g_isInSession){
        std::string ipStr = NetworkManager::getLocalIP() + ":" + std::to_string(g_network->m_port);
        m_ipLabel->setString(ipStr.c_str());
        m_helpLabel->setVisible(true);
    }

    // port input
    auto portLabel = CCLabelBMFont::create("Port:","bigFont.fnt");
    portLabel->setScale(0.5f);
    portLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 60
    ));
    this->m_mainLayer->addChild(portLabel);

    m_portInput = TextInput::create(100.0f, std::to_string(g_network->m_port), "chatFont.fnt");
    m_portInput->setFilter("1234567890");
    m_portInput->setString(std::to_string(g_network->m_port));
    m_portInput->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 30
    ));
    this->m_mainLayer->addChild(m_portInput);

    // password input
    auto passLabel = CCLabelBMFont::create("Password:","bigFont.fnt");
    passLabel->setScale(0.5f);
    passLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2
    ));
    this->m_mainLayer->addChild(passLabel);

    m_passInput = TextInput::create(100.0f, "Leave blank for no password", "chatFont.fnt");
    m_passInput->setFilter("qwertyuiopasdfghjklzxcvbnm1234567890,.-@!_");
    m_passInput->setString(g_network->getPassword());
    m_passInput->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 - 35
    ));
    this->m_mainLayer->addChild(m_passInput);
    m_passInput->setCallback([this](const std::string& text){
        log::info("meow pass changed to {}", text);
        g_network->setPassword(text);
    }
    );

    // host button
    hostBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Host", "goldFont.fnt", "GJ_button_01.png", .8f),
        this,
        menu_selector(HostPopup::onStartHost)
    );
    hostBtn->setPosition(ccp(
        winSize.width/2,
        winSize.height/2-80
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

    if (g_network->host(port, m_passInput->getString())){
        if (hostBtn){
            hostBtn->setSprite(ButtonSprite::create("Stop Hosting", "goldFont.fnt", "GJ_button_01.png", .8f));
        }
        g_isHost = true;
        g_isInSession = true;

        g_sync->setUserID(g_network->getPeerID());

        std::string ipStr = NetworkManager::getLocalIP() + ":" + std::to_string(port);
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