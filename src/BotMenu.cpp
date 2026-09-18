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
        s_current->keyBackClicked();
    }
}

void BotMenuPopup::onClose(CCObject* pSender) {
    if (s_current == this) {
        s_current = nullptr;
    }
    Popup::onClose(pSender);
}

bool BotMenuPopup::init() {
    if (!Popup::init(260.f, 240.f)) return false;

    s_current = this;
    this->setTitle("MacroBot");

    auto winSize = m_mainLayer->getContentSize();

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(menu);

    // --- Record button (top-left) ------------------------------------
    auto recordLabel = ButtonSprite::create("Record", "bigFont.fnt", "GJ_button_01.png", 0.75f);
    m_recordBtn = CCMenuItemSpriteExtra::create(
        recordLabel, this, menu_selector(BotMenuPopup::onRecord)
    );
    m_recordBtn->setPosition({winSize.width * 0.5f - 60.f, winSize.height * 0.68f});
    menu->addChild(m_recordBtn);

    // --- Save button (top-right) --------------------------------------
    auto saveLabel = ButtonSprite::create("Save", "bigFont.fnt", "GJ_button_02.png", 0.75f);
    m_saveBtn = CCMenuItemSpriteExtra::create(
        saveLabel, this, menu_selector(BotMenuPopup::onSave)
    );
    m_saveBtn->setPosition({winSize.width * 0.5f + 60.f, winSize.height * 0.68f});
    menu->addChild(m_saveBtn);

    // --- Play button (mid-left) ------------------------------------
    auto playLabel = ButtonSprite::create("Play", "bigFont.fnt", "GJ_button_01.png", 0.75f);
    m_playBtn = CCMenuItemSpriteExtra::create(
        playLabel, this, menu_selector(BotMenuPopup::onPlay)
    );
    m_playBtn->setPosition({winSize.width * 0.5f - 60.f, winSize.height * 0.46f});
    menu->addChild(m_playBtn);

    // --- Load button (mid-right) -----------------------------------
    auto loadLabel = ButtonSprite::create("Load", "bigFont.fnt", "GJ_button_01.png", 0.75f);
    auto loadBtn = CCMenuItemSpriteExtra::create(
        loadLabel, this, menu_selector(BotMenuPopup::onLoad)
    );
    loadBtn->setPosition({winSize.width * 0.5f + 60.f, winSize.height * 0.46f});
    menu->addChild(loadBtn);

    // --- Debug HUD toggle (shows in-level frame/event counters) --------
    auto offSpr = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png");
    auto onSpr = CCSprite::createWithSpriteFrameName("GJ_checkOn_001.png");
    m_hudToggle = CCMenuItemToggler::create(
        offSpr, onSpr, this, menu_selector(BotMenuPopup::onToggleHud)
    );
    m_hudToggle->setScale(0.7f);
    m_hudToggle->setPosition({winSize.width * 0.5f - 60.f, winSize.height * 0.27f});
    menu->addChild(m_hudToggle);

    auto hudLabel = CCLabelBMFont::create("Debug HUD", "chatFont.fnt");
    hudLabel->setScale(0.5f);
    hudLabel->setAnchorPoint({0.f, 0.5f});
    hudLabel->setPosition({winSize.width * 0.5f - 45.f, winSize.height * 0.27f});
    m_mainLayer->addChild(hudLabel);

    // --- Status label ----------------------------------------------
    m_statusLabel = CCLabelBMFont::create("", "chatFont.fnt");
    m_statusLabel->setScale(0.5f);
    m_statusLabel->setPosition({winSize.width * 0.5f, winSize.height * 0.12f});
    m_statusLabel->setID("macrobot-status-label");
    m_mainLayer->addChild(m_statusLabel);

    refreshButtonStates();
    return true;
}

void BotMenuPopup::refreshButtonStates() {
    auto& mgr = MacroManager::get();
    auto mode = mgr.mode();

    if (m_recordBtn) {
        if (auto spr = typeinfo_cast<ButtonSprite*>(m_recordBtn->getNormalImage())) {
            spr->setString(mode == Mode::Recording ? "Stop" : "Record");
        }
    }

    if (m_saveBtn) {
        bool canSave = mgr.hasPendingSave();
        m_saveBtn->setEnabled(canSave);
        m_saveBtn->setOpacity(canSave ? 255 : 120);
    }

    if (m_playBtn) {
        m_playBtn->setEnabled(mgr.hasArmedMacro());
        m_playBtn->setOpacity(mgr.hasArmedMacro() ? 255 : 120);
    }

    if (m_hudToggle) {
        m_hudToggle->toggle(mgr.isDebugHudEnabled());
    }

    if (m_statusLabel) {
        std::string status;
        switch (mode) {
            case Mode::Recording: status = "Recording... play the attempt"; break;
            case Mode::Standby:   status = "Attempt captured - tap Save"; break;
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
    auto mode = mgr.mode();

    if (mode == Mode::Recording || mode == Mode::Standby) {
        // Starting over discards whatever was captured/pending.
        mgr.cancelRecording();
    }

    if (auto pl = PlayLayer::get()) {
        if (auto level = pl->m_level) {
            mgr.setLevelName(level->m_levelName);
        }
    }
    mgr.startRecording();

    enterPracticeAndResume();
    this->keyBackClicked();
}

void BotMenuPopup::onSave(CCObject*) {
    MacroManager::get().saveStandbyMacro();
    refreshButtonStates();
}

void BotMenuPopup::onPlay(CCObject*) {
    beginPlayback();
    this->keyBackClicked();
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
