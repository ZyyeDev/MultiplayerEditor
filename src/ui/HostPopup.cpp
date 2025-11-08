#include "HostPopup.hpp"
#include "../network/NetworkManager.hpp"
#include "../sync/SyncManager.hpp"

extern NetworkManager* g_network;
extern SyncManager* g_sync;

extern bool g_isHost;
extern bool g_isInSession;

HostPopup* HostPopup::create(){
    auto ret = new HostPopup();
    if (ret->initAnchored(320.0f,280.0f)){
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool HostPopup::setup(){
    this->setTitle("Host Session");

    auto winSize = this->m_mainLayer->getContentSize();

    // info text
    auto infoLabel = CCLabelBMFont::create("Host IP:", "bigFont.fnt");
    infoLabel->setScale(.4f);
    infoLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 40
    ));
    this->m_mainLayer->addChild(infoLabel);

    // ip label
    m_ipLabel = CCLabelBMFont::create("(Host To Show IP)","chatFont.fnt");
    m_ipLabel->setScale(.6f);
    m_ipLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 60
    ));
    this->m_mainLayer->addChild(m_ipLabel);

    // host button
    hostBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Host", "goldFont.fnt", "GJ_button_01.png", .8f),
        this,
        menu_selector(HostPopup::onStartHost)
    );
    hostBtn->setPosition(ccp(
        winSize.width/2,
        winSize.height/2+30
    ));

    if (g_isHost && g_isInSession){
        m_ipLabel->setString("127.0.0.1:7777");
    }

    // popup menu
    auto menu = CCMenu::create();
    menu->addChild(hostBtn);
    menu->setPosition(0, 0);
    this->m_mainLayer->addChild(menu);

    return true;
}

void HostPopup::onStartHost(CCObject*){
    // TODO: The user should be able to chose the port they are hosting
    if (g_network->host(7777)){
        g_isHost = true;
        g_isInSession = true;

        g_sync->setUserID("host");

        // for now, just show local ip
        // it should show local ip, or hamachi ip (or whatever the user is using)
        m_ipLabel->setString("127.0.0.1:7777");

        log::info("it works!!! :3");
        
        // TODO
        /*if (hostBtn){
            hostBtn->setSprite(
                ButtonSprite::create("Stop Hosting", "goldFont.fnt", "GJ_button_01.png", .8f)
            );
        }*/

        FLAlertLayer::create(
            "Hosting!",
            "You are now hosting!",
            "OK"
        )->show();

        g_network->setOnConnect([]() {
            log::info("Client connected");

            if (g_sync){
                g_sync->sendFullState();
            } else {
                // i dont think this can happen, but just in case. No one knows.
                FLAlertLayer::create(
                    "ERROR",
                    "Couldn't send full state: g_sync is null! Please report.",
                    "OK"
                )->show();
            }
        });
    } else {
        FLAlertLayer::create(
            "Error",
            "Failed to host!",
            "OK"
        )->show();
    }
}