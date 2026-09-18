#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <vector>

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
    struct Fields {
        CCLabelBMFont* frameLabel = nullptr;
        CCLabelBMFont* eventsLabel = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (level) {
            MacroManager::get().setLevelName(level->m_levelName);
        }

        // Toggleable debug HUD: current tick counter + event counter, so
        // it's actually visible what recording/playback is doing instead
        // of it being an opaque black box. Created hidden/shown up front
        // and just toggled/updated from update() below.
        m_fields->frameLabel = CCLabelBMFont::create("Frame: 0", "chatFont.fnt");
        m_fields->frameLabel->setAnchorPoint({0.f, 1.f});
        m_fields->frameLabel->setScale(0.45f);
        m_fields->frameLabel->setID("macrobot-frame-label"_spr);
        m_fields->frameLabel->setZOrder(1000);
        this->addChild(m_fields->frameLabel, 1000);

        m_fields->eventsLabel = CCLabelBMFont::create("Events: 0", "chatFont.fnt");
        m_fields->eventsLabel->setAnchorPoint({0.f, 1.f});
        m_fields->eventsLabel->setScale(0.45f);
        m_fields->eventsLabel->setID("macrobot-events-label"_spr);
        m_fields->eventsLabel->setZOrder(1000);
        this->addChild(m_fields->eventsLabel, 1000);

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        m_fields->frameLabel->setPosition({6.f, winSize.height - 6.f});
        m_fields->eventsLabel->setPosition({6.f, winSize.height - 20.f});

        updateDebugHud();
        return true;
    }

    // Called once per rendered frame. `dt` is fed into MacroManager's
    // fixed-rate accumulator, which advances a virtual clock (0, 1, or
    // several ticks depending on how much time actually elapsed) — see
    // MacroManager::onPhysicsStep for why this replaced counting update()
    // calls directly (that was the actual cause of playback not working).
    void update(float dt) {
        PlayLayer::update(dt);

        MacroManager::get().onPhysicsStep(dt, [this](uint8_t button, bool player1, bool down) {
            // Feed the recorded input back into the game exactly the way
            // a real press/release would: through handleButton. This is
            // the same entry point used for touch/keyboard input, so the
            // physics/response is identical to a human playing it live.
            this->handleButton(down, button, player1);
        });

        updateDebugHud();
    }

    void updateDebugHud() {
        auto& mgr = MacroManager::get();
        bool show = mgr.isDebugHudEnabled();

        if (m_fields->frameLabel) m_fields->frameLabel->setVisible(show);
        if (m_fields->eventsLabel) m_fields->eventsLabel->setVisible(show);
        if (!show) return;

        m_fields->frameLabel->setString(("Frame: " + std::to_string(mgr.currentStep())).c_str());

        std::string eventsText;
        switch (mgr.mode()) {
            case Mode::Recording:
                eventsText = "Events: " + std::to_string(mgr.recordedEventCount()) + " (recording)";
                break;
            case Mode::Standby:
                eventsText = "Events: " + std::to_string(mgr.recordedEventCount()) + " (standby, unsaved)";
                break;
            case Mode::Playing:
                eventsText = "Events: " + std::to_string(mgr.playedEventCount()) + "/"
                              + std::to_string(mgr.totalArmedEventCount()) + " (playing)";
                break;
            default:
                eventsText = "Events: " + std::to_string(mgr.totalArmedEventCount()) + " armed";
                break;
        }
        m_fields->eventsLabel->setString(eventsText.c_str());
    }

    void handleButton(bool down, int button, bool isPlayer1) {
        // Record BEFORE calling through, so the timing recorded matches
        // the tick we were just ticked on. This is a no-op unless we're
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
// PauseLayer has no single documented "button menu" field to hook into
// (its bindings only expose two bool fields, m_unfocused/m_tryingQuit —
// confirmed against the generated bindings; an earlier revision of this
// mod assumed an `m_buttonMenu` member that doesn't actually exist here).
// So rather than guess at internal layout members/IDs again, we do real
// collision detection: walk every existing CCMenuItem already in the
// pause layer (recursively, since they live inside several sub-menus —
// resume/retry/quit, practice/normal mode, replay, settings, etc.),
// compute their on-screen rects, and place our button at the first free
// slot going down the right edge. That's what actually fixes "button
// lands on place 1 when a button is already there" for any GD version,
// without depending on a specific member or node ID existing.
// -----------------------------------------------------------------------

// Returns `node`'s bounding box converted into world (screen) space, by
// chaining convertToWorldSpace up through its parent. boundingBox() is
// already expressed in the parent's coordinate system.
static CCRect worldRectOf(CCNode* node) {
    auto rect = node->boundingBox();
    auto parent = node->getParent();
    if (!parent) return rect;
    auto bottomLeft = parent->convertToWorldSpace({rect.getMinX(), rect.getMinY()});
    auto topRight = parent->convertToWorldSpace({rect.getMaxX(), rect.getMaxY()});
    return CCRect(bottomLeft.x, bottomLeft.y, topRight.x - bottomLeft.x, topRight.y - bottomLeft.y);
}

// Recursively gathers the world-space rects of every clickable button
// already present under `root` (pause menu buttons live in several
// nested CCMenus, not just one).
static void collectButtonRects(CCNode* root, std::vector<CCRect>& out) {
    if (!root) return;
    auto children = root->getChildren();
    if (!children) return;
    for (auto child : CCArrayExt<CCNode*>(children)) {
        if (auto item = typeinfo_cast<CCMenuItem*>(child)) {
            out.push_back(worldRectOf(item));
        }
        collectButtonRects(child, out);
    }
}

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

        std::vector<CCRect> existingButtons;
        collectButtonRects(this, existingButtons);

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        const float halfSize = 26.f;   // ~ our button's half-width after scale
        const float stepDown = 48.f;   // vertical gap between candidate slots
        CCPoint pos = {winSize.width - 35.f, winSize.height - 35.f};

        auto overlapsExisting = [&](const CCPoint& p) {
            CCRect probe(p.x - halfSize, p.y - halfSize, halfSize * 2.f, halfSize * 2.f);
            for (auto& r : existingButtons) {
                if (r.intersectsRect(probe)) return true;
            }
            return false;
        };

        // Walk down the right edge until we land on an unoccupied slot —
        // "place 2" instead of overlapping whatever's at "place 1".
        for (int i = 0; i < 12 && overlapsExisting(pos); i++) {
            pos.y -= stepDown;
        }

        auto menu = CCMenu::create();
        menu->setID("macrobot-menu"_spr);
        menu->addChild(btn);
        menu->setPosition(pos);
        menu->setZOrder(100);
        this->addChild(menu);
    }

    void onOpenBotMenu(CCObject*) {
        BotMenuPopup::create()->show();
    }
};
