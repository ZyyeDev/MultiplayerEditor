#include "Popups.hpp"

bool JoinSessionPopup::setup(){
    auto winSize = CCDirector::sharedDirector()->getWinSize();
    this->setTitle("Join Session");

    return true;
}

void JoinSessionPopup::onConnect(CCObject *){
    this->onClose(nullptr);
}

JoinSessionPopup* JoinSessionPopup::create(){
    auto ret = new JoinSessionPopup();
    if (ret && ret->initAnchored(300,200)){
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}