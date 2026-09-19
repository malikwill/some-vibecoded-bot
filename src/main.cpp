#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
#include <Geode/binding/UILayer.hpp>
#include <vector>

#include "MacroManager.hpp"
#include "BotMenu.hpp"

using namespace geode::prelude;
using namespace macrobot;

// -----------------------------------------------------------------------
// Input capture/injection: GJBaseGameLayer::handleButton, NOT
// PlayLayer::handleButton.
//
// This was the actual root cause of playback (and possibly recording)
// not working at all. GD's real handleButton virtual lives on
// GJBaseGameLayer — PlayLayer inherits it but doesn't re-declare it —
// confirmed against a known-working reference macro bot, which
// consistently hooks $modify(GJBaseGameLayer) and calls through via
// GJBaseGameLayer::handleButton(...). Hooking it on PlayLayer compiled
// fine (Geode didn't complain) but silently never intercepted real
// input, since the vtable slot the game actually calls belongs to
// GJBaseGameLayer's binding, not a same-named method placed on PlayLayer.
//
// Also note the third parameter's real polarity is `player2` (true means
// it's player 2's input), not `player1` — inverted from what earlier
// revisions of this mod assumed. That mismatch wouldn't break a normal
// single-player level (the bit is unused there) but matters for 2P/dual.
// -----------------------------------------------------------------------

class $modify(MacroBotBaseGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool player2) {
        if (PlayLayer::get()) {
            // Only stores while actively Recording; no-op otherwise
            // (including during Playing, so fed-back input isn't
            // re-recorded, and during Standby).
            MacroManager::get().recordInput(button, /*player1=*/!player2, down);
        }
        GJBaseGameLayer::handleButton(down, button, player2);
    }
};

// -----------------------------------------------------------------------
// Physics-accurate frame counter: PlayerObject::update, NOT
// PlayLayer::update.
//
// PlayLayer::update(dt) fires once per RENDERED frame. GD's actual
// physics run in several fixed-size substeps per rendered frame — and
// PlayerObject::update(stepDelta) is what fires once per real substep
// (confirmed against the same reference bot, which drives its own
// frame-accurate logic off exactly this hook). Counting rendered frames
// instead of real substeps was the other half of why recorded timing
// never matched actual gameplay physics.
//
// Guarded to player 1 only: in 2-player/dual mode both PlayerObjects get
// an update() call within the same substep, so also counting player 2
// would double-count every tick.
// -----------------------------------------------------------------------

class $modify(MacroBotPlayerObject, PlayerObject) {
    void update(float stepDelta) {
        auto* pl = PlayLayer::get();
        if (pl && this == pl->m_player1) {
            // Fire due playback input BEFORE running this substep's
            // physics, so it takes effect in the same substep a live
            // touch would have (touches dispatch before update() calls
            // in cocos2d's frame loop).
            MacroManager::get().onPhysicsStep([pl](uint8_t button, bool isPlayer1, bool down) {
                pl->handleButton(down, button, /*player2=*/!isPlayer1);
            });
        }
        PlayerObject::update(stepDelta);
    }
};

// -----------------------------------------------------------------------
// PlayLayer hooks: level lifecycle + the toggleable debug HUD.
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

        // Debug HUD labels go on UILayer, not directly on PlayLayer.
        // PlayLayer's own coordinate space scrolls/scales with the level
        // camera, so a child added straight to `this` drifts off-screen
        // almost immediately — that's why the HUD wasn't showing anything
        // at all. UILayer is GD's own fixed, non-scrolling overlay (it's
        // what the percentage/attempt labels live on), so it's the
        // correct parent for a screen-pinned overlay.
        CCNode* hudParent = UILayer::get();
        if (!hudParent) hudParent = this;

        m_fields->frameLabel = CCLabelBMFont::create("Frame: 0", "chatFont.fnt");
        m_fields->frameLabel->setAnchorPoint({0.f, 1.f});
        m_fields->frameLabel->setScale(0.45f);
        m_fields->frameLabel->setID("macrobot-frame-label"_spr);
        hudParent->addChild(m_fields->frameLabel, 1000);

        m_fields->eventsLabel = CCLabelBMFont::create("Events: 0", "chatFont.fnt");
        m_fields->eventsLabel->setAnchorPoint({0.f, 1.f});
        m_fields->eventsLabel->setScale(0.45f);
        m_fields->eventsLabel->setID("macrobot-events-label"_spr);
        hudParent->addChild(m_fields->eventsLabel, 1000);

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        m_fields->frameLabel->setPosition({6.f, winSize.height - 6.f});
        m_fields->eventsLabel->setPosition({6.f, winSize.height - 20.f});

        updateDebugHud();
        return true;
    }

    // Only used to refresh the HUD's text once per rendered frame now —
    // the actual playback/recording timing lives in PlayerObject::update
    // above.
    void update(float dt) {
        PlayLayer::update(dt);
        updateDebugHud();
    }

    void updateDebugHud() {
        auto& mgr = MacroManager::get();
        bool show = mgr.isDebugHudEnabled();

        if (m_fields->frameLabel) m_fields->frameLabel->setVisible(show);
        if (m_fields->eventsLabel) m_fields->eventsLabel->setVisible(show);
        if (!show) return;

        m_fields->frameLabel->setString(("Frame: " + std::to_string(mgr.currentFrame())).c_str());

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

    void resetLevel() {
        PlayLayer::resetLevel();
        // If we were Recording, this is "the attempt/session finished":
        // MacroManager moves to Standby (keeps the buffer, stops
        // capturing further attempts) until the user taps Save.
        MacroManager::get().onLevelReset();
    }

    void onQuit() {
        // No auto-save: leaving the level while Recording or on Standby
        // (unsaved) discards whatever was captured. Saving only ever
        // happens explicitly via the Save button.
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
// PauseLayer has no documented "button menu" field (confirmed against
// the generated bindings: it only exposes m_unfocused/m_tryingQuit as
// fields), so placement can't rely on a member or a guessed node ID.
//
// The button is now placed via TWO layers of care:
//  1. Placement is deferred one frame (scheduleOnce) so it runs AFTER
//     every mod's customSetup() has finished adding its own buttons this
//     init cycle — otherwise our collision scan can miss a button a
//     later-running mod (in hook-priority order) adds this same frame,
//     which is what actually caused landing on top of an existing
//     third-party button before.
//  2. Placement is computed from the existing buttons near our default
//     corner (not a fixed offset guess): we take the lowest edge of
//     whatever's already clustered there and sit just below it, so a
//     4th button lands after a 3rd rather than needing to guess spacing.
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
// under `root` that falls within `radius` of `anchor` — pause menu
// buttons live in several nested CCMenus, and we only care about the
// ones clustered near our own default corner, not GD's whole button set
// (its native resume/retry/quit row, for instance, is nowhere near it).
static void collectNearbyButtonRects(CCNode* root, const CCPoint& anchor, float radius, std::vector<CCRect>& out) {
    if (!root) return;
    auto children = root->getChildren();
    if (!children) return;
    for (auto child : CCArrayExt<CCNode*>(children)) {
        if (auto item = typeinfo_cast<CCMenuItem*>(child)) {
            auto rect = worldRectOf(item);
            CCPoint center = {rect.getMidX(), rect.getMidY()};
            float dx = center.x - anchor.x;
            float dy = center.y - anchor.y;
            if (dx * dx + dy * dy <= radius * radius) {
                out.push_back(rect);
            }
        }
        collectNearbyButtonRects(child, anchor, radius, out);
    }
}

class $modify(MacroBotPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();
        // Defer to next frame so every other mod's customSetup() (this
        // whole synchronous hook chain) has already run and added its
        // own buttons before we scan for free space.
        this->scheduleOnce(schedule_selector(MacroBotPauseLayer::placeBotButton), 0.0f);
    }

    void placeBotButton(float) {
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

        auto winSize = CCDirector::sharedDirector()->getWinSize();
        CCPoint corner = {winSize.width - 35.f, winSize.height - 35.f};

        std::vector<CCRect> nearby;
        collectNearbyButtonRects(this, corner, 170.f, nearby);

        const float halfSize = 26.f;
        CCPoint pos = corner;

        if (!nearby.empty()) {
            float lowestY = nearby.front().getMinY();
            for (auto& r : nearby) {
                lowestY = std::min(lowestY, r.getMinY());
            }
            // Sit just below whatever's already stacked in this corner —
            // "next slot in the column" instead of a fixed guessed offset.
            pos = {corner.x, lowestY - halfSize - 6.f};
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
