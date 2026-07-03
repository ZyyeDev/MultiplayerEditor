#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

class HostPopup : public Popup{
    protected:
        CCLabelBMFont* m_ipLabel;
        CCLabelBMFont* m_helpLabel;
        CCMenuItemSpriteExtra* hostBtn;
        TextInput* m_portInput;
        TextInput* m_passInput;
        
        bool init() override;
        void onStartHost(CCObject*);
    public:
        static HostPopup* create();
};