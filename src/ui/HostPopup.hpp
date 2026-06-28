#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

class HostPopup : public Popup{
    protected:
        CCLabelBMFont* m_ipLabel;
        CCMenuItemSpriteExtra* hostBtn;
        TextInput* m_portInput;
        
        bool init() override;
        void onStartHost(CCObject*);
    public:
        static HostPopup* create();
};