#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>

#include "MacroManager.hpp"
#include "BotMenu.hpp"

using namespace geode::prelude;
using namespace macrobot;

// -----------------------------------------------------------------------
// PlayLayer hooks: this is where recording / playback actually happen.
//
// NOTE ON GAME VERSION BINDINGS
// This mod targets GD 2.2081 / Geode SDK 5.10.1 / Android x64. The
// function signatures below (update, handleButton, resetLevel, onQuit)
// match PlayLayer/GJBaseGameLayer across recent GD versions, but if
// `geode build` reports a signature mismatch for your exact bindings
// package, open the generated Geode bindings for PlayLayer /
// GJBaseGameLayer and adjust the signatures here to match — the logic
// in MacroManager does not need to change, only these hook signatures.
// -----------------------------------------------------------------------

class $modify(MacroBotPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (level) {
            MacroManager::get().setLevelName(level->m_levelName);
        }
        return true;
    }

    // Called once per physics tick — the exact, fixed-rate granularity we
    // key every recorded input to. This is what makes playback
    // frame-perfect regardless of the FPS the level is later replayed at.
    void update(float dt) {
        PlayLayer::update(dt);

        MacroManager::get().onPhysicsStep([this](uint8_t button, bool player1, bool down) {
            // Feed the recorded input back into the game exactly the way
            // a real press/release would: through handleButton. This is
            // the same entry point used for touch/keyboard input, so the
            // physics/response is identical to a human playing it live.
            this->handleButton(down, button, player1);
        });
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        // Record BEFORE calling through, so the timing recorded matches
        // the step we were just ticked on.
        MacroManager::get().recordInput(button, isPlayer1, down);
        PlayLayer::handleButton(down, button, isPlayer1);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        MacroManager::get().onLevelReset();
    }

    void onQuit() {
        // Leaving the level (back to the level select / editor) is what
        // we treat as "the practice session is finished" — auto-save
        // whatever was being recorded, named after the level.
        if (MacroManager::get().mode() == Mode::Recording) {
            MacroManager::get().finishRecordingAndSave();
        }
        if (MacroManager::get().mode() == Mode::Playing) {
            MacroManager::get().stopPlaying();
        }
        PlayLayer::onQuit();
    }
};

// -----------------------------------------------------------------------
// PauseLayer hook: adds the circular button that opens the bot menu.
// -----------------------------------------------------------------------

class $modify(MacroBotPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto winSize = CCDirector::sharedDirector()->getWinSize();

        auto sprite = CCSprite::create("botButton.png"_spr);
        if (!sprite) {
            // Resource failed to load for some reason — fall back to a
            // stock sprite so the mod still functions.
            sprite = CCSprite::createWithSpriteFrameName("GJ_editorBtn_001.png");
        }
        sprite->setScale(0.9f);

        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(MacroBotPauseLayer::onOpenBotMenu)
        );
        btn->setID("macrobot-open-btn"_spr);

        auto menu = CCMenu::create();
        menu->setID("macrobot-menu"_spr);
        menu->addChild(btn);
        menu->setPosition({winSize.width - 35.f, winSize.height - 35.f});
        menu->setZOrder(100);
        this->addChild(menu);
    }

    void onOpenBotMenu(CCObject*) {
        BotMenuPopup::create()->show();
    }
};
