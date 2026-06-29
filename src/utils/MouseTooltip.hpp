#pragma once

#include <Geode/Geode.hpp>
#include <vector>
#include <string>

using namespace geode::prelude;

struct TooltipRegion {
    geode::WeakRef<cocos2d::CCNode> node;
    std::string text;
    ccColor3B color;
};

class MouseTooltip {
    public:
        static MouseTooltip* get();

        void registerRegion(CCNode* node, const std::string& text, ccColor3B color = {255, 255, 255});
        void unregisterRegion(CCNode* node);
        void clear();

        void update();

    private:
        std::vector<TooltipRegion> m_regions;
        CCLabelBMFont* m_label = nullptr;
        CCLayerColor* m_bg = nullptr;

        void ensureLabel();
        void showTooltip(const std::string& text, ccColor3B color, CCPoint pos);
        void hideTooltip();
};
