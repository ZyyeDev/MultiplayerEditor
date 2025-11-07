#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

class JoinSessionPopup : public geode::Popup<> {
    protected:
        bool setup() override;
        void onConnect(CCObject*);
    public:
        static JoinSessionPopup* create();
};
