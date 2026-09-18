#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <filesystem>
#include <vector>

namespace macrobot {

// Lists every .mbf file in the mod's save dir. Picking one loads it and
// immediately auto-clicks Play (per spec: "it gets back to the bot menu,
// auto-clicks once the play button").
class LoadPopup : public geode::Popup<> {
protected:
    bool setup() override;
    void onPick(cocos2d::CCObject* sender);

    std::vector<std::filesystem::path> m_files;

public:
    static LoadPopup* create();
};

}
