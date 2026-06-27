#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

class JoinSessionPopup : public geode::Popup{
public:
    static JoinSessionPopup* create();
protected:
    bool init() override;
    void onConnect(CCObject*);
};
