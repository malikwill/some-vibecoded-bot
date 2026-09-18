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
        // the step we were just ticked on. This is a no-op unless we're
        // actively in Recording mode — in particular it does nothing
        // while on Standby (an attempt already captured, waiting on Save)
        // or during Playing (fed-back input isn't re-recorded).
        MacroManager::get().recordInput(button, isPlayer1, down);
        PlayLayer::handleButton(down, button, isPlayer1);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        // If we were Recording, this is "the attempt/session finished":
        // MacroManager moves to Standby (keeps the buffer, stops
        // capturing further attempts) until the user taps Save.
        MacroManager::get().onLevelReset();
    }

    void onQuit() {
        // No auto-save anymore: leaving the level while Recording or on
        // Standby (unsaved) discards whatever was captured. Saving is
        // only ever done explicitly via the Save button.
        auto mode = MacroManager::get().mode();
        if (mode == Mode::Recording || mode == Mode::Standby) {
            MacroManager::get().cancelRecording();
        }
        if (mode == Mode::Playing) {
            MacroManager::get().stopPlaying();
        }
        PlayLayer::onQuit();
    }
};

// -----------------------------------------------------------------------
// PauseLayer hook: adds the circular button that opens the bot menu.
//
// Added directly into PauseLayer's own `m_buttonMenu` (rather than a
// separate menu positioned at a hardcoded corner) so it takes its place
// in the existing button row/grid instead of overlapping whatever GD
// already put in that corner. `m_buttonMenu` has a Layout assigned
// (Row/Grid depending on GD version) that re-flows every child's
// position in order each time `updateLayout()` is called — so our button
// lands in the next free slot automatically.
// -----------------------------------------------------------------------

class $modify(MacroBotPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

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

        if (m_buttonMenu) {
            m_buttonMenu->addChild(btn);
            // Re-flows every button in m_buttonMenu (existing ones plus
            // ours) according to its Layout, so ours takes the next open
            // slot instead of sitting wherever a fixed position would
            // have placed it.
            m_buttonMenu->updateLayout();
        } else {
            // Extremely unlikely fallback: no button menu found at all.
            // Place independently so the mod still works.
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            auto menu = CCMenu::create();
            menu->setID("macrobot-menu"_spr);
            menu->addChild(btn);
            menu->setPosition({winSize.width - 35.f, winSize.height - 35.f});
            this->addChild(menu);
        }
    }

    void onOpenBotMenu(CCObject*) {
        BotMenuPopup::create()->show();
    }
};
