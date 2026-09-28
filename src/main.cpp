#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/binding/GJGameLevel.hpp>
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
// Fixed-timestep normalization: GJBaseGameLayer::update(float dt).
//
// This is the actual root cause of severe playback drift (worst on
// ship/wave, per report). GD computes how many physics substeps to run
// per update() call with roughly:
//   steps = max(1, round(dt * 240 / min(timeWarp, 1.0)))
// — confirmed against a technical write-up from the developer of
// Silicate (a well-known GD bot), who documented this exact formula from
// disassembly. `dt` here is whatever the device's real frame-to-frame
// time happens to be at that instant — not something we otherwise
// control — so the number of substeps GD produces for a given moment of
// gameplay depends on the actual rendering framerate at the time. A
// recording session and a later playback session essentially never share
// identical frame timing, so they can produce a genuinely different
// substep sequence for the same span of level, even though the
// PlayerObject::update hook below faithfully counts whatever substeps
// actually happen. Continuous, hold-sensitive modes (ship, wave) show
// this worst because position is an integral of held-button state, so
// any substep-count mismatch compounds every tick; cube's jumps are more
// self-correcting (landing resets vertical state).
//
// Fix: accumulate the real incoming dt and feed GJBaseGameLayer's real
// update() in fixed 1/240s slices — the classic "fix your timestep"
// pattern, adapted specifically to line up with GD's own rounding
// formula above (feeding exactly 1/240 makes GD's own computation
// resolve to exactly one substep whenever timeWarp == 1). This makes the
// resulting substep sequence a deterministic function of the level's own
// timeWarp state only, not of real device frame timing, while still
// advancing overall game time at the real accumulated rate (unlike a TAS
// tool decoupling simulation from wall-clock time entirely — this mod
// wants normal real-time gameplay pace, just with deterministic physics
// resolution underneath it).
// -----------------------------------------------------------------------

class $modify(MacroBotFixedStep, GJBaseGameLayer) {
    struct Fields {
        double accumulator = 0.0;
    };

    void update(float dt) {
        constexpr double kFixedDt = 1.0 / 240.0;

        // Clamp so a lag spike or the app being backgrounded doesn't
        // queue up a huge catch-up burst of ticks in one real frame.
        double clamped = dt;
        if (clamped > 0.25) clamped = 0.25;
        if (clamped < 0.0) clamped = 0.0;

        m_fields->accumulator += clamped;

        while (m_fields->accumulator >= kFixedDt) {
            m_fields->accumulator -= kFixedDt;
            GJBaseGameLayer::update(static_cast<float>(kFixedDt));
        }
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

            // Feeds the level-time->frame log used to figure out which
            // frame a later checkpoint respawn should roll back to (see
            // MacroManager::onLevelReset). Keyed on PlayLayer::m_gameState
            // ::m_levelTime rather than X position — confirmed against a
            // working reference bot, which uses exactly this field for its
            // own tick/checkpoint bookkeeping, specifically because it's
            // guaranteed monotonically increasing during real gameplay
            // (unlike X position, which isn't on a backward-scrolling
            // level). No-op outside Recording.
            MacroManager::get().logTime(pl->m_gameState.m_levelTime);

            // Piggyback the debug HUD's refresh on this same hook rather
            // than a separate PlayLayer::update(dt) hook: PlayLayer does
            // NOT itself declare update(dt) (confirmed against the full
            // generated member list — it's only inherited), which is the
            // same "declared on an ancestor, so a $modify(PlayLayer) hook
            // silently never fires" issue that handleButton had. This
            // hook is already proven working (it's what makes playback
            // work at all), so it's the safe place to drive the HUD too.
            // Routed through MacroManager rather than a method on the
            // PlayLayer $modify class itself — a method added there isn't
            // actually part of PlayLayer's real interface as seen from
            // this (different) $modify class, which is what "no member
            // named updateDebugHud in PlayLayer" was.
            MacroManager::get().updateHud();
        }
        PlayerObject::update(stepDelta);
    }
};

// -----------------------------------------------------------------------
// PlayLayer hooks: level lifecycle + the toggleable debug HUD.
// -----------------------------------------------------------------------

class $modify(MacroBotPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        if (level) {
            MacroManager::get().setLevelName(level->m_levelName);
        }

        // Debug HUD labels are created lazily by MacroManager itself
        // (see MacroManager::ensureHudLabels), parented to the scene
        // root rather than to PlayLayer — nothing to do here. This
        // removes any dependency on PlayLayer's own init timing/
        // coordinate space, which is what an earlier revision assumed
        // (incorrectly blamed for the HUD not showing) and what led to
        // routing everything through MacroManager in the first place
        // (a method added to a $modify(PlayLayer) class isn't visible
        // from PlayerObject's separate $modify class).
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        // Synchronous notification (not deferred): if we're Playing (or
        // about to start), stop onPhysicsStep from firing anything more
        // until this reset settles — see
        // MacroManager::notifyResetLevelCalled for the accuracy bug this
        // closes (playback's first few events, or the first few after a
        // mid-playback retry, firing twice).
        MacroManager::get().notifyResetLevelCalled();

        // Reading the player's position immediately here was the actual
        // bug behind "never goes to standby": it isn't necessarily
        // settled into its final post-reset spot synchronously within
        // this call yet (still reflecting wherever the player died),
        // which made every reset — including genuine restarts — read as
        // "far from the start" and get misclassified as a checkpoint
        // respawn. Check one frame later instead, by which point the
        // position has actually caught up.
        //
        // Cancel any previously-scheduled check first: resetLevel() can
        // fire more than once in quick succession (observed during
        // level/practice-mode startup — entering practice mode alone can
        // trigger it, sometimes more than once), and a STALE pending
        // check firing later — after real gameplay has already resumed
        // and moved on — was misreading ongoing play as a fresh reset
        // event, corrupting the recording ("events weirdly changed",
        // nothing actually saving). Only the latest resetLevel() call's
        // check should ever run.
        log::info("MacroBot: [resetLevel] fired (m_isPracticeMode={}) — scheduling reset-kind check for next frame",
                   this->m_isPracticeMode);
        this->unschedule(schedule_selector(MacroBotPlayLayer::checkLevelResetKind));
        this->scheduleOnce(schedule_selector(MacroBotPlayLayer::checkLevelResetKind), 0.0f);
    }

    void checkLevelResetKind(float) {
        // Practice mode determines whether this reset is still part of
        // the same recording session. A death at the level start is
        // indistinguishable from a manual restart by position (or level
        // time) alone, so neither is used as the signal to enter Standby
        // on its own — see MacroManager::onLevelReset.
        double respawnLevelTime = this->m_gameState.m_levelTime;
        log::info("MacroBot: [checkLevelResetKind] respawnLevelTime={:.4f} practiceModeNow={}",
                   respawnLevelTime, this->m_isPracticeMode);
        MacroManager::get().onLevelReset(respawnLevelTime, this->m_isPracticeMode);
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
        // Tear down the HUD labels — they're attached to this level's
        // scene, which is about to go away.
        MacroManager::get().clearHud();
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
        sprite->setScale(1.26f); // 40% bigger than the original 0.9

        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(MacroBotPauseLayer::onOpenBotMenu)
        );
        btn->setID("macrobot-open-btn"_spr);

        CCPoint corner = {35.f, 35.f}; // bottom-left

        std::vector<CCRect> nearby;
        collectNearbyButtonRects(this, corner, 170.f, nearby);

        const float halfSize = 36.f; // scaled up to match the bigger button
        CCPoint pos = corner;

        if (!nearby.empty()) {
            float highestY = nearby.front().getMaxY();
            for (auto& r : nearby) {
                highestY = std::max(highestY, r.getMaxY());
            }
            // Sit just above whatever's already stacked in this corner —
            // going up (not down, which would run off-screen from a
            // bottom-left anchor) is the "next slot in the column".
            pos = CCPoint(corner.x, highestY + halfSize + 6.f);
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
