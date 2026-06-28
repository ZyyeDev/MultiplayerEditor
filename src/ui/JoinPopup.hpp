#pragma once

#include <Geode/Geode.hpp>

using namespace geode::prelude;

class JoinPopup : public Popup{
    protected:
        TextInput* m_ipInput;
        TextInput* m_portInput;
        
        bool init() override;
        void OnJoin(CCObject*);
    public:
        static JoinPopup* create();
};
