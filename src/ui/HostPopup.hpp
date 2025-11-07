#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

class HostPopup : public Popup<>{
    protected:
        CCLabelBMFont* m_ipLabel;
        CCMenuItemSpriteExtra* hostBtn;

        bool setup() override;
        void onStartHost(CCObject*);
    public:
        static HostPopup* create();
};