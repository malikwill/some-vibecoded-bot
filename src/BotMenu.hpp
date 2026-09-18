#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

namespace macrobot {

// Shared by BotMenuPopup's Play button and LoadPopup's pick handler:
// resets the current level and arms the manager to play back whatever
// macro is currently loaded/armed. Closes any open MacroBot popups.
void beginPlayback();

// The popup that opens when the circular button in the pause menu is
// pressed. Three buttons: Record, Play, Load.
class BotMenuPopup : public geode::Popup<> {
protected:
    bool setup() override;
    void refreshButtonStates();

    void onRecord(cocos2d::CCObject*);
    void onPlay(cocos2d::CCObject*);
    void onLoad(cocos2d::CCObject*);

    cocos2d::CCMenuItemSpriteExtra* m_recordBtn = nullptr;
    cocos2d::CCMenuItemSpriteExtra* m_playBtn = nullptr;
    geode::TextArea* m_statusLabel = nullptr;

public:
    static BotMenuPopup* create();
};

}
