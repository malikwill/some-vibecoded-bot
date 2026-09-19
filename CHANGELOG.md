# Changelog

## v1.1.0-beta.5
- Fixed a compile error in `placeBotButton()`: `pos = {corner.x, ...};`
  (reassigning an already-declared `CCPoint` from a braced-init-list) hit
  an ambiguous `operator=` overload resolution on this toolchain. Replaced
  with an explicit `CCPoint(...)` constructor call, which sidesteps it.
  This is a different code path from the `setPosition({...})` calls
  elsewhere (those pass a braced list as a function argument, resolved
  directly against the parameter type, not through `operator=`) and from
  brace-init at declaration (uses the constructor, not assignment) — both
  of those remain fine as written.

## v1.1.0-beta.4 (major update — playback fix, take 2)
Reworked using a real, working reference implementation (a friend's
backend macro bot) as a guide — found two concrete, verified bugs that
account for the total playback failure, not just a timing-precision
issue:

- **Root cause #1: wrong hook target for input.** GD's real
  `handleButton` virtual lives on `GJBaseGameLayer`, not `PlayLayer` —
  `PlayLayer` inherits it but doesn't redeclare it. Hooking it on
  `PlayLayer` compiled without error but never actually intercepted real
  input. Now hooked as `$modify(GJBaseGameLayer)`, matching the verified
  reference. Also corrected the third parameter's polarity: it's
  `player2` (true = second player), not `player1` as previously assumed
  — harmless on a single-player level, but wrong for 2P/dual mode.
- **Root cause #2: wrong hook target for timing.** `PlayLayer::update()`
  fires once per *rendered frame*; GD's actual physics run several fixed
  substeps per rendered frame, and `PlayerObject::update(stepDelta)` is
  what fires once per real substep. Recording/playback now key off that
  instead, giving a `frame` counter that matches real physics steps
  instead of an approximation of them. Firing due playback input before
  calling through to `PlayerObject::update` (rather than after) also
  matches how a live touch lands before physics runs for that step.
- **`.mbf` reworked to `{frame, button, state, player}`** (format v3):
  `frame` = physical substep index (uint32), `button` = GD button id,
  `state` = pressed/released, `player` = player1/player2. Dropped the
  earlier tick-rate/accumulator metadata — the fixed-substep-hook
  approach makes it unnecessary.
- **Fixed the debug HUD showing nothing.** The labels were parented
  directly to `PlayLayer`, whose coordinate space scrolls/scales with
  the level camera — so they were being dragged off-screen immediately.
  Now parented to `UILayer` (GD's own fixed, non-scrolling overlay,
  where the percentage/attempt labels live).
- **Button placement, second attempt.** Still no reliance on a guessed
  `PauseLayer` member/ID. Two changes: placement is now deferred one
  frame (`scheduleOnce`) so it runs after every other mod's
  `customSetup()` has already added its buttons this init cycle (a
  same-frame ordering race was likely why a button landed on top of an
  existing one before); and instead of probing fixed candidate points,
  it now finds the actual lowest edge of whatever's already clustered
  near the default corner and sits directly below it.

## v1.1.0-beta.3 (major update — playback fix)
- **Fixed macro playback not working at all.** The actual bug: `m_step`
  was incremented once per `PlayLayer::update()` call — but `update()`
  fires once per rendered frame, at whatever framerate the device
  happens to be running. A recording session and a later playback
  session never produce the same sequence/timing of `update()` calls
  (different FPS, vsync jitter, lag), so the recorded step indices never
  lined up with playback's step indices — inputs fired at the wrong
  moments or not at all.
- Replaced it with the same approach real `.gdr`/`.gdr2`-style replay
  formats use: a **fixed-rate virtual clock** (`MacroManager::kTicksPerSecond`,
  240 ticks/sec) that `update()`'s real `dt` is accumulated into, advancing
  0, 1, or several ticks per call as time actually elapses. Both recording
  and playback are now keyed to this same FPS-independent clock instead
  of to `update()` call count.
- `.mbf` format bumped to version 2: now embeds `ticksPerSecond` in the
  header (metadata for forward-compatibility, matching how real replay
  formats record their own clock rate) and warns in the log if a loaded
  macro's rate doesn't match the current build's.
- Added a **toggleable debug HUD** (checkbox in the bot menu) showing a
  live frame counter and an event counter in the top-left of the level —
  event count while recording, played/total while playing, armed count
  otherwise — so it's actually visible what recording/playback is doing.

## v1.1.0-beta.2
- Fixed the build error: `PauseLayer` has no `m_buttonMenu` field —
  confirmed against the generated bindings, it only exposes
  `m_unfocused`/`m_tryingQuit`. That assumption from beta.1 is gone.
- Replaced it with a placement approach that needs no guessed member or
  node ID at all: `customSetup()` now recursively collects the on-screen
  rects of every existing `CCMenuItem` in the pause layer and places our
  button at the first free slot walking down the right edge — real
  collision detection, which is what actually fixes "lands on place 1
  when something's already there."
- README's bindings note updated accordingly, and corrected a stale line
  still describing the old Android64/Windows/macOS multi-platform build
  (this mod has been Android64-only since beta.2 of the 1.0.0 line).

## v1.1.0-beta.1 (major update)
- **Recording flow reworked**, replacing auto-save:
  - Record now auto-enters practice mode and resumes gameplay.
  - When the attempt/session ends (level reset), capturing stops and the
    buffer moves to a new **Standby** state — further attempts are not
    recorded.
  - Added a **Save** button (enabled only on Standby) that writes the
    captured attempt to disk and arms it for Play. Nothing is saved
    automatically anymore.
  - Tapping Record again while Recording or on Standby discards whatever
    was pending and starts a fresh capture.
  - Leaving the level (onQuit) while Recording or on Standby now discards
    the buffer instead of auto-saving it.
- **Pause-menu button placement fixed**: the bot button is now added
  directly into `PauseLayer::m_buttonMenu` and the menu's `updateLayout()`
  is called afterward, so it takes the next open slot in GD's own button
  row/grid instead of sitting at a fixed corner that could overlap an
  existing button.
- `MacroManager` gained a `Mode::Standby` state; `finishRecordingAndSave()`
  was replaced by `saveStandbyMacro()`.
- README updated to describe the new Record → (attempt ends) → Save flow,
  and to flag the two added binding assumptions
  (`PlayLayer::m_isPracticeMode`, `PauseLayer::onPracticeMode`).

## v1.0.0-beta.4
- Fixed the real remaining build error: `Popup`'s `onClose()` is virtual
  with a `CCObject*` parameter (inherited from `FLAlertLayer`), not the
  no-arg version the SDK docs page implied. `BotMenuPopup::onClose` now
  matches that signature and forwards the sender through to `Popup::onClose`.
- Added the missing `override` keyword to both popups' `init()` (they
  genuinely override `CCNode::init()`) to clear an
  `-Winconsistent-missing-override` warning.

## v1.0.0-beta.3
- `mod.json`: pinned the required Geode loader version (`"geode"` field)
  to `5.10.1`, matching the target SDK. Previously left at a placeholder
  `4.4.0`, which also risks the CI build resolving against loader APIs
  older than what the Popup fixes in beta.2 assume.

## v1.0.0-beta.2
- Fixed build failure: ported `BotMenuPopup`/`LoadPopup` from the old
  templated `geode::Popup<>` API to the current non-template
  `geode::Popup` (own `init()` override instead of `setup()`,
  `Popup::init(w, h)`, manual `create()`).
- Fixed unqualified-type errors for `CCMenuItemSpriteExtra` (isn't inside
  `cocos2d::`) and removed an unused, non-existent `geode::TextArea` field.
- Popups now close via `keyBackClicked()` instead of the old `onClose(nullptr)`
  call pattern; `BotMenuPopup::onClose()` is now properly overridden to
  clear its tracked instance.
- Loading a macro from `LoadPopup` now also closes the bot menu behind it
  before starting playback, matching "load -> back to bot menu -> auto-play".
- CI: dropped the Windows/macOS build jobs. This mod's `mod.json` only
  declares a `gd.android` version, so those platforms failed at CMake
  configure time (`JSON member 'gd win'/'gd mac' not found`). Only
  Android64 is built now, matching the original Android x64 target.

## v1.0.0-beta.1
- Initial release.
- Circular bot-menu button added to the pause layer.
- Record / Play / Load popup.
- Physics-step-accurate recording and playback via `PlayLayer::handleButton`.
- New `.mbf` macro format (accuracy-over-size, one entry per input
  transition, keyed to physics step index).
- Auto-save on leaving a level while recording, named after the level.
- Load popup lists saved macros and auto-plays the one you pick.
- GitHub Actions workflow building Android64/Windows/macOS and combining
  into a single `.geode` release artifact.
