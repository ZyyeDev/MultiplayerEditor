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

    // Address input (host:port)
    auto addrLabel = CCLabelBMFont::create("Address (host:port):","bigFont.fnt");
    addrLabel->setScale(0.5f);
    addrLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 90
    ));
    this->m_mainLayer->addChild(addrLabel);

    m_addressInput = TextInput::create(200.0f, "127.0.0.1:8080", "chatFont.fnt");
    m_addressInput->setFilter("qwertyuiopasdfghjklzxcvbnm1234567890,.-@!_:");
    m_addressInput->setString("127.0.0.1:8080");
    m_addressInput->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 60
    ));
    this->m_mainLayer->addChild(m_addressInput);

    // Password Input
    auto passLabel = CCLabelBMFont::create("Password:","bigFont.fnt");
    passLabel->setScale(0.5f);
    passLabel->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 + 10
    ));
    this->m_mainLayer->addChild(passLabel);

    m_passInput = TextInput::create(200.0f, "Leave blank for no password", "chatFont.fnt");
    m_passInput->setFilter("qwertyuiopasdfghjklzxcvbnm1234567890,.-@!_");
    m_passInput->setString("");
    m_passInput->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 - 20
    ));
    this->m_mainLayer->addChild(m_passInput);

    // Join button
    auto joinBtn = CCMenuItemSpriteExtra::create(
        ButtonSprite::create("Join","goldFont.fnt","GJ_button_01.png",0.8f),
        this,
        menu_selector(JoinPopup::OnJoin)
    );
    joinBtn->setPosition(ccp(
        winSize.width/2,
        winSize.height/2 - 65
    ));

    auto menu = CCMenu::create();
    menu->addChild(joinBtn);
    menu->setPosition(0,0);
    this->m_mainLayer->addChild(menu);

    return true;
}

void JoinPopup::OnJoin(CCObject*) {
    std::string address = m_addressInput->getString();
    std::string password = m_passInput->getString();

    if (address.empty()){
        FLAlertLayer::create("Error","Please enter a valid address!!", "OK")->show();
        return;
    }

    std::string ip;
    uint16_t port = g_network->m_port;

    // Parse host:port
    auto colonPos = address.find(':');
    if (colonPos != std::string::npos){
        ip = address.substr(0, colonPos);
        std::string portStr = address.substr(colonPos + 1);
        try {
            port = static_cast<uint16_t>(std::stoi(portStr));
        } catch (...) {
            port = g_network->m_port;
        }
    } else {
        ip = address;
    }

    if (ip.empty()){
        FLAlertLayer::create("Error","Please enter a valid address!!", "OK")->show();
        return;
    }

    log::info("Attempting to join: {}:{}", ip, port);

    auto loadcircle = LoadingCircle::create();
    auto bg = CCLayerColor::create({0,0,0,180});
    bg->setContentSize(CCDirector::get()->getWinSize());
    this->addChild(bg);

    CCTouchDispatcher::get()->setDispatchEvents(false);
    loadcircle->setParentLayer(bg);
    loadcircle->show();

    std::thread([loadcircle, bg, ip, port, password]() {
        bool connected = g_network->connect(ip, port, password);

        geode::queueInMainThread(
            [loadcircle, bg, connected]() {
                loadcircle->fadeAndRemove();
                bg->removeFromParent();
                CCTouchDispatcher::get()->setDispatchEvents(true);

                if (!connected) {
                    FLAlertLayer::create(
                        "Connection Failed",
                        "Couldn't connect to host!",
                        "OK"
                    )->show();
                    return;
                }

                auto level = GJGameLevel::create();
                if (!level) {
                    FLAlertLayer::create(
                        "Error",
                        "Couldn't create the session level!",
                        "OK"
                    )->show();
                    return;
                }

                g_isHost = false;
                g_isInSession = true;
                g_sync->setUserID(g_network->getPeerID());

                level->m_levelName = "Collab Session";
                level->m_dontSave = true;
                level->m_levelIndex = -1;
                level->m_levelID = 0;
                level->m_levelType = GJLevelType::Editor;

                auto editorLayer = LevelEditorLayer::create(level, false);
                if (!editorLayer) {
                    FLAlertLayer::create(
                        "Error",
                        "Couldn't initialize the session editor!",
                        "OK"
                    )->show();
                    return;
                }

                auto scene = CCScene::create();
                scene->addChild(editorLayer);

                CCDirector::sharedDirector()->replaceScene(
                    CCTransitionFade::create(0.5f, scene)
                );
            }
        );
    }).detach();
}