#include "BotMenu.hpp"
#include "LoadPopup.hpp"
#include "MacroManager.hpp"
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PauseLayer.hpp>

using namespace geode::prelude;

namespace macrobot {

// Finds an active PauseLayer in the scene (if the bot menu was opened
// from the pause screen).
static PauseLayer* findPauseLayer() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->getParent()) return nullptr;

    for (auto child : CCArrayExt<CCNode*>(pl->getParent()->getChildren())) {
        if (auto pause = typeinfo_cast<PauseLayer*>(child)) {
            return pause;
        }
    }

    return nullptr;
}

// Resumes gameplay if we're currently paused.
static void resumeIfPaused() {
    if (auto pause = findPauseLayer()) {
        pause->onResume(nullptr);
    }
}

// Record should "automatically go into practice mode and start the
// session": if we're not already in practice mode, clicking GD's own
// practice-mode pause button both enables it and resumes gameplay
// (exactly like a manual click would). If practice mode is already on,
// just resume.
static void enterPracticeAndResume() {
    auto pl = PlayLayer::get();
    auto pause = findPauseLayer();

    if (!pl || !pause) return;

    if (!pl->m_isPracticeMode) {
        pause->onPracticeMode(nullptr);
    } else {
        pause->onResume(nullptr);
    }
}

void beginPlayback() {
    auto& mgr = MacroManager::get();

    if (!mgr.hasArmedMacro()) return;

    mgr.startPlaying();

    if (auto pl = PlayLayer::get()) {
        // Restart the attempt from the beginning so the macro's recorded
        // step indices line up with the level's actual start.
        pl->resetLevel();
    }

    resumeIfPaused();
}

BotMenuPopup* BotMenuPopup::s_current = nullptr;

BotMenuPopup* BotMenuPopup::create() {
    auto ret = new BotMenuPopup();

    if (ret->init()) {
        ret->autorelease();
        return ret;
    }

    delete ret;
    return nullptr;
}

void BotMenuPopup::closeIfOpen() {
    if (s_current) {
        s_current->closeSelf();
    }
}

void BotMenuPopup::closeSelf() {
    if (s_current == this) {
        s_current = nullptr;
    }

    this->setKeypadEnabled(false);
    this->removeFromParentAndCleanup(true);
}

void BotMenuPopup::keyBackClicked() {
    closeSelf();
}

void BotMenuPopup::onCloseClicked(CCObject*) {
    closeSelf();
}

static constexpr float kPanelWidth = 260.f;
static constexpr float kPanelHeight = 240.f;

bool BotMenuPopup::init() {
    if (!CCLayer::init()) return false;

    this->setContentSize({kPanelWidth, kPanelHeight});

    // Background "card" — a plain, self-drawn panel rather than
    // anything borrowed from Popup/FLAlertLayer, so its position always
    // matches this layer's own position exactly.
    auto bg = CCScale9Sprite::create("GJ_square01.png");
    bg->setContentSize({kPanelWidth, kPanelHeight});
    bg->setAnchorPoint({0.f, 0.f});
    bg->setPosition({0.f, 0.f});
    this->addChild(bg);

    auto title = CCLabelBMFont::create("MacroBot", "goldFont.fnt");
    title->setScale(0.9f);
    title->setPosition({kPanelWidth * 0.5f, kPanelHeight - 22.f});
    this->addChild(title);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    this->addChild(menu);

    // --- Close button (top-right corner) --------------------------------
    auto closeSprite = CCSprite::createWithSpriteFrameName("GJ_closeBtn_001.png");
    auto closeBtn = CCMenuItemSpriteExtra::create(
        closeSprite,
        this,
        menu_selector(BotMenuPopup::onCloseClicked)
    );
    closeBtn->setPosition({kPanelWidth - 16.f, kPanelHeight - 16.f});
    menu->addChild(closeBtn);

    // --- Record button (top-left) ------------------------------------
    auto recordLabel = ButtonSprite::create(
        "Record",
        "bigFont.fnt",
        "GJ_button_01.png",
        0.75f
    );

    m_recordBtn = CCMenuItemSpriteExtra::create(
        recordLabel,
        this,
        menu_selector(BotMenuPopup::onRecord)
    );

    m_recordBtn->setPosition({
        kPanelWidth * 0.5f - 60.f,
        kPanelHeight * 0.68f
    });

    menu->addChild(m_recordBtn);

    // --- Save button (top-right) --------------------------------------
    auto saveLabel = ButtonSprite::create(
        "Save",
        "bigFont.fnt",
        "GJ_button_02.png",
        0.75f
    );

    m_saveBtn = CCMenuItemSpriteExtra::create(
        saveLabel,
        this,
        menu_selector(BotMenuPopup::onSave)
    );

    m_saveBtn->setPosition({
        kPanelWidth * 0.5f + 60.f,
        kPanelHeight * 0.68f
    });

    menu->addChild(m_saveBtn);

    // --- Play button (mid-left) ------------------------------------
    auto playLabel = ButtonSprite::create(
        "Play",
        "bigFont.fnt",
        "GJ_button_01.png",
        0.75f
    );

    m_playBtn = CCMenuItemSpriteExtra::create(
        playLabel,
        this,
        menu_selector(BotMenuPopup::onPlay)
    );

    m_playBtn->setPosition({
        kPanelWidth * 0.5f - 60.f,
        kPanelHeight * 0.46f
    });

    menu->addChild(m_playBtn);

    // --- Load button (mid-right) -----------------------------------
    auto loadLabel = ButtonSprite::create(
        "Load",
        "bigFont.fnt",
        "GJ_button_01.png",
        0.75f
    );

    auto loadBtn = CCMenuItemSpriteExtra::create(
        loadLabel,
        this,
        menu_selector(BotMenuPopup::onLoad)
    );

    loadBtn->setPosition({
        kPanelWidth * 0.5f + 60.f,
        kPanelHeight * 0.46f
    });

    menu->addChild(loadBtn);

    // --- Debug HUD toggle (shows in-level frame/event counters) --------
    auto offSpr = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
    auto onSpr = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");

    m_hudToggle = CCMenuItemToggler::create(
        offSpr,
        onSpr,
        this,
        menu_selector(BotMenuPopup::onToggleHud)
    );

    m_hudToggle->setScale(0.7f);
    m_hudToggle->setPosition({
        kPanelWidth * 0.5f - 60.f,
        kPanelHeight * 0.27f
    });

    menu->addChild(m_hudToggle);

    auto hudLabel = CCLabelBMFont::create("Debug HUD", "chatFont.fnt");
    hudLabel->setScale(0.5f);
    hudLabel->setAnchorPoint({0.f, 0.5f});
    hudLabel->setPosition({
        kPanelWidth * 0.5f - 45.f,
        kPanelHeight * 0.27f
    });
    this->addChild(hudLabel);

    // --- Status label ----------------------------------------------
    m_statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_statusLabel->setScale(0.5f);
    m_statusLabel->setPosition({
        kPanelWidth * 0.5f,
        kPanelHeight * 0.12f
    });
    m_statusLabel->setID("macrobot-status-label");
    this->addChild(m_statusLabel);

    refreshButtonStates();
    return true;
}

void BotMenuPopup::show() {
    s_current = this;

    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    auto winSize = CCDirector::sharedDirector()->getWinSize();
    auto size = this->getContentSize();

    // Centered: anchor is (0,0) (default CCLayer), so the position we
    // set is the panel's bottom-left corner — placing it at
    // (winSize - panelSize) / 2 puts the panel's actual center on the
    // screen's actual center.
    this->setPosition({
        (winSize.width - size.width) * 0.5f,
        (winSize.height - size.height) * 0.5f
    });

    this->setZOrder(10000);
    scene->addChild(this, 10000);

    this->setKeypadEnabled(true);
    refreshButtonStates();
}

void BotMenuPopup::refreshButtonStates() {
    auto& mgr = MacroManager::get();
    auto mode = mgr.mode();

    if (m_recordBtn) {
        if (auto spr = typeinfo_cast<ButtonSprite*>(m_recordBtn->getNormalImage())) {
            spr->setString(
                mode == Mode::Recording ? "Stop" : "Record"
            );
        }
    }

    if (m_saveBtn) {
        bool canSave = mgr.hasPendingSave();
        m_saveBtn->setEnabled(canSave);
        m_saveBtn->setOpacity(canSave ? 255 : 120);
    }

    if (m_playBtn) {
        m_playBtn->setEnabled(mgr.hasArmedMacro());
        m_playBtn->setOpacity(
            mgr.hasArmedMacro() ? 255 : 120
        );
    }

    if (m_hudToggle) {
        m_hudToggle->toggle(mgr.isDebugHudEnabled());
    }

    if (m_statusLabel) {
        std::string status;

        switch (mode) {
            case Mode::Recording:
                status = "Recording... play the attempt";
                break;

            case Mode::Standby:
                status = "Attempt captured - tap Save";
                break;

            default: {
                if (auto name = mgr.armedMacroName()) {
                    status = "Loaded: " + *name;
                } else {
                    status = "No macro loaded";
                }
                break;
            }
        }

        m_statusLabel->setString(status.c_str());
    }
}

void BotMenuPopup::onRecord(CCObject*) {
    auto& mgr = MacroManager::get();

    if (mgr.mode() == Mode::Recording) {
        // The button is displayed as "Stop" while recording. Stop should
        // preserve the captured attempt and move it to Standby so the
        // player can save it, rather than cancelling it and immediately
        // starting another recording.
        mgr.stopRecording();
        refreshButtonStates();
        return;
    }

    if (mgr.mode() == Mode::Standby) {
        // Pressing Record again after an unsaved attempt explicitly
        // discards that attempt and starts a fresh recording.
        mgr.cancelRecording();
    }

    if (auto pl = PlayLayer::get()) {
        if (auto level = pl->m_level) {
            mgr.setLevelName(level->m_levelName);
        }
    }

    mgr.startRecording();

    enterPracticeAndResume();
    closeSelf();
}

void BotMenuPopup::onSave(CCObject*) {
    MacroManager::get().saveStandbyMacro();
    refreshButtonStates();
}

void BotMenuPopup::onPlay(CCObject*) {
    beginPlayback();
    closeSelf();
}

void BotMenuPopup::onLoad(CCObject*) {
    LoadPopup::create()->show();
}

void BotMenuPopup::onToggleHud(CCObject*) {
    auto& mgr = MacroManager::get();
    mgr.setDebugHudEnabled(!mgr.isDebugHudEnabled());

    // CCMenuItemToggler already flips its own visual state on click; we
    // don't need to touch m_hudToggle here.
}

}
