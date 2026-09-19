#pragma once
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

using namespace geode::prelude;

namespace macrobot {

// Shared by BotMenuPopup's Play button and LoadPopup's pick handler:
// resets the current level and arms the manager to play back whatever
// macro is currently loaded/armed.
void beginPlayback();

// The popup that opens when the circular button in the pause menu is
// pressed. Four buttons (Record, Save, Play, Load) plus a Debug HUD
// toggle that shows/hides the in-level frame + event counters.
//
// NOTE: current Geode (5.x) ui/Popup.hpp exposes a plain, non-template
// `geode::Popup` (subclass of FLAlertLayer) — not the older `Popup<Args...>`
// template. You override `init()` yourself (calling `Popup::init(w, h)`
// first) rather than a `setup()` virtual. See docs.geode-sdk.org/tutorials/popup.
class BotMenuPopup : public geode::Popup {
protected:
    bool init() override;
    void onClose(CCObject* pSender) override;
    void onEnter() override;
    void refreshButtonStates();

    void onRecord(CCObject*);
    void onSave(CCObject*);
    void onPlay(CCObject*);
    void onLoad(CCObject*);
    void onToggleHud(CCObject*);

    CCMenuItemSpriteExtra* m_recordBtn = nullptr;
    CCMenuItemSpriteExtra* m_saveBtn = nullptr;
    CCMenuItemSpriteExtra* m_playBtn = nullptr;
    CCMenuItemToggler* m_hudToggle = nullptr;
    CCLabelBMFont* m_statusLabel = nullptr;

    static BotMenuPopup* s_current;

public:
    static BotMenuPopup* create();
    // Closes the currently-open bot menu, if any. Used by LoadPopup so
    // picking a macro visibly returns to the bot menu before it auto-
    // closes via Play (matches the spec: load -> back to bot menu ->
    // auto-click play).
    static void closeIfOpen();
};

}
