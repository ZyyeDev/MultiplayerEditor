#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Label.hpp>

using namespace geode::prelude;

class HostPopup : public Popup{
    protected:
        geode::Label* m_ipLabel;
        geode::Label* m_helpLabel;
        CCMenuItemSpriteExtra* hostBtn;
        TextInput* m_portInput;
        TextInput* m_passInput;
        
        bool init() override;
        void onStartHost(CCObject*);
    public:
        static HostPopup* create();
};