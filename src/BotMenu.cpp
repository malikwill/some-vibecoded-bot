#include "BotMenu.hpp"
#include "LoadPopup.hpp"
#include "MacroManager.hpp"
#include <Geode/binding/PlayLayer.hpp>
#include <Geode/binding/PauseLayer.hpp>

using namespace geode::prelude;

namespace macrobot {

// Finds an active PauseLayer in the scene (if the bot menu was opened
// from the pause screen) and resumes gameplay, so Play actually starts
// running immediately instead of leaving the game frozen behind popups.
static void resumeIfPaused() {
    auto pl = PlayLayer::get();
    if (!pl || !pl->getParent()) return;
    for (auto child : CCArrayExt<CCNode*>(pl->getParent()->getChildren())) {
        if (auto pause = typeinfo_cast<PauseLayer*>(child)) {
            pause->onResume(nullptr);
            return;
        }
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

void BotMenuPopup::onClose() {
    if (s_current == this) {
        s_current = nullptr;
    }
    Popup::onClose();
}

bool BotMenuPopup::init() {
    if (!Popup::init(240.f, 190.f)) return false;

    s_current = this;
    this->setTitle("MacroBot");

    auto winSize = m_mainLayer->getContentSize();
    auto& mgr = MacroManager::get();

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(menu);

    // --- Record button ---------------------------------------------------
    auto recordLabel = ButtonSprite::create("Record", "bigFont.fnt", "GJ_button_01.png", 0.8f);
    m_recordBtn = CCMenuItemSpriteExtra::create(
        recordLabel, this, menu_selector(BotMenuPopup::onRecord)
    );
    m_recordBtn->setPosition({winSize.width * 0.5f, winSize.height * 0.62f});
    menu->addChild(m_recordBtn);

    // --- Play button -------------------------------------------------------
    auto playLabel = ButtonSprite::create("Play", "bigFont.fnt", "GJ_button_01.png", 0.8f);
    m_playBtn = CCMenuItemSpriteExtra::create(
        playLabel, this, menu_selector(BotMenuPopup::onPlay)
    );
    m_playBtn->setPosition({winSize.width * 0.5f - 55.f, winSize.height * 0.32f});
    menu->addChild(m_playBtn);

    // --- Load button -------------------------------------------------------
    auto loadLabel = ButtonSprite::create("Load", "bigFont.fnt", "GJ_button_01.png", 0.8f);
    auto loadBtn = CCMenuItemSpriteExtra::create(
        loadLabel, this, menu_selector(BotMenuPopup::onLoad)
    );
    loadBtn->setPosition({winSize.width * 0.5f + 55.f, winSize.height * 0.32f});
    menu->addChild(loadBtn);

    // --- Status label --------------------------------------------------
    std::string status = "No macro loaded";
    if (auto name = mgr.armedMacroName()) {
        status = "Loaded: " + *name;
    }
    auto label = CCLabelBMFont::create(status.c_str(), "chatFont.fnt");
    label->setScale(0.55f);
    label->setPosition({winSize.width * 0.5f, winSize.height * 0.15f});
    label->setID("macrobot-status-label");
    m_mainLayer->addChild(label);

    refreshButtonStates();
    return true;
}

void BotMenuPopup::refreshButtonStates() {
    auto& mgr = MacroManager::get();
    bool recording = (mgr.mode() == Mode::Recording);

    if (m_recordBtn) {
        if (auto spr = typeinfo_cast<ButtonSprite*>(m_recordBtn->getNormalImage())) {
            spr->setString(recording ? "Stop" : "Record");
        }
    }
    if (m_playBtn) {
        m_playBtn->setEnabled(mgr.hasArmedMacro());
        m_playBtn->setOpacity(mgr.hasArmedMacro() ? 255 : 120);
    }
}

void BotMenuPopup::onRecord(CCObject*) {
    auto& mgr = MacroManager::get();
    if (mgr.mode() == Mode::Recording) {
        mgr.cancelRecording();
    } else {
        if (auto pl = PlayLayer::get()) {
            if (auto level = pl->m_level) {
                mgr.setLevelName(level->m_levelName);
            }
        }
        mgr.startRecording();
    }
    refreshButtonStates();
}

void BotMenuPopup::onPlay(CCObject*) {
    beginPlayback();
    this->keyBackClicked();
}

void BotMenuPopup::onLoad(CCObject*) {
    LoadPopup::create()->show();
}

}
