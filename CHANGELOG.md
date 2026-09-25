# Changelog

## v1.1.0-beta.16
The "dying once still puts it on Standby" bug is fixed — but not by my
approach. You found a better one: use `PlayLayer::m_isPracticeMode`
(GD's own authoritative state) as the signal for whether a reset still
belongs to the same recording session, instead of trying to infer it
purely from position deltas.

- `MacroManager::onLevelReset` now takes an explicit `practiceMode`
  parameter. While practice mode is active, *every* reset — a checkpoint
  respawn or a death before the first checkpoint (which lands back at
  the start, same as a manual restart would) — stays part of the same
  session: a checkpoint respawn rolls the frame/buffer back to that
  checkpoint, and a reset at the start rolls back to frame 0, but
  neither ends the recording. The session only actually finishes once
  practice mode is no longer active.
- This replaces the position-distance-only heuristic that kept
  misclassifying a death-at-the-start (indistinguishable from a manual
  restart by position alone) as "genuine restart, end the session" —
  which was the real root cause all along.
- Added detailed logging through the whole decision path on top of this
  (tagged `MacroBot: [reset]` / `[log]` / `[checkLevelResetKind]` /
  `[resetLevel]`): the raw `respawnX` and `m_isPracticeMode` read at each
  step, every `resetLevel()` firing, the session's captured starting X,
  and on every reset — the mode, both positions, the computed delta,
  which branch it took, and before/after event counts for whichever trim
  ran. Useful for any future edge case in this area; grep the Geode log
  for `MacroBot:` after reproducing something to see the full trace.

## v1.1.0-beta.15
- `resources/logo.png` was 2KB (256×256, mostly flat color) and Geode's
  build rejected it — regenerated larger and with real detail (radial
  gradient background, circuit-line accents, a soft-shadowed play
  triangle) at 384×384, landing at ~25KB, within the expected range.

## v1.1.0-beta.14
- Fixed the "started a new session on another level, inputs at the end
  never saved, event count changed weirdly" bug. `resetLevel()` can fire
  more than once in quick succession (observed during level/practice-
  mode startup — entering practice mode alone can trigger it, sometimes
  more than once), and each call scheduled its own deferred position
  check one frame later. A stale pending check from an earlier call
  could end up firing AFTER real gameplay had already resumed and moved
  on, misreading ongoing play as a fresh reset event and corrupting the
  recording. `resetLevel()` now cancels any previously-scheduled check
  before scheduling a new one, so only the latest call's check ever runs.
- Fixed the events counter going up on death even though death itself
  isn't a press/release. GD force-releases any button the player was
  still holding as part of its own death cleanup, which fires a real
  `handleButton(false, ...)` call — indistinguishable at the moment it
  happens from a deliberate release, but it's cleanup, not something the
  player did. Unlike an unreleased trailing press (already trimmed since
  beta.12), this looks like a perfectly normal, properly-paired release,
  so it needed its own check: on a genuine restart, any trailing event
  landing within a couple of frames of the reset itself is now also
  trimmed before the macro is finalized.

## v1.1.0-beta.13
- `mod.json`: set the real mod id (`itzmalikhere.macrobot`) and developer
  (`malik`), replacing the `yourname` placeholders. `LICENSE` updated to
  match.

## v1.1.0-beta.12
- Fixed "one death immediately puts it on Standby" — a second, distinct
  bug from last time's fix. Pressing Record calls `enterPracticeAndResume()`,
  and if practice mode wasn't already active, entering it fires its own
  internal `resetLevel()` — before a single position sample has ever
  been logged (`m_haveStartX` still false at that point). The classifier
  short-circuits `m_haveStartX && ...` to `false` whenever it's false,
  which was making that very first, entirely automatic reset get read as
  "genuine restart" every time — ending the session before the player
  had even started playing. Now, a reset with no position history yet is
  just absorbed (stays Recording, nothing to roll back), and the actual
  session-start position gets established by the next real position
  sample once gameplay resumes.
- Fixed a manual restart counting as a spurious recorded event. GD's
  hold-jump-to-restart gesture (holding the jump button down triggers a
  restart) generates a real `handleButton(true, ...)` call, indistinguishable
  from any other press at the moment it happens — so it was getting
  recorded like normal gameplay input even though it isn't any. On a
  genuine restart, the buffer is now trimmed first: for each
  (button, player) pair, if its very last recorded event is a press with
  no matching release after it, that press is dropped before the macro
  is finalized. Covers both the restart gesture and a jump that's simply
  cut short by death (also not meaningful to keep).

## v1.1.0-beta.11
- Fixed "never goes to Standby" from last time. The actual bug: reading
  the player's position immediately after calling through to
  `PlayLayer::resetLevel()` isn't reliable — it doesn't appear to be
  settled into its final post-reset spot synchronously within that same
  call (still reading as wherever the player died a moment earlier).
  Since that position is almost never close to the session's start, a
  genuine restart was getting misread as "far from start" every time —
  exactly like a checkpoint respawn — so the "session finished" branch
  never ran and nothing ever moved to Standby.
  Fixed by deferring the check one frame (`scheduleOnce`, the same
  pattern already used elsewhere in this mod), by which point the
  position has actually caught up to its real post-reset value.

## v1.1.0-beta.10
Your report that the debug HUD's frame counter zeroed out on every
single death in practice mode (not just genuine restarts) turned out to
reveal a real, previously-unhandled case: `PlayLayer::resetLevel()`
fires for BOTH a checkpoint respawn (practice mode, dying with a
checkpoint already placed — the level doesn't actually restart, it just
jumps back to a mid-level position) and a genuine restart-to-the-
beginning, and this mod was treating every single one of them as "the
session finished", ending the recording after the very first death.

- `MacroManager` now tells the two apart by comparing the player's X
  position right after the reset against the position recorded at the
  start of the session: landing back at (approximately) the start means
  a genuine restart (session finished, as before); landing meaningfully
  further in means a checkpoint respawn.
- On a checkpoint respawn, recording now **continues** instead of ending
  — the frame counter and the recorded buffer are rolled back to
  whatever frame the player was at when they were last at (about) that
  position (tracked via a lightweight position→frame log built during
  recording), discarding only the events from the attempt that just
  died. The debug HUD's frame counter now reflects that rollback too,
  instead of zeroing.
- A genuine restart still clears everything and moves to Standby exactly
  as before — only checkpoint respawns get the new behavior.

## v1.1.0-beta.9 (major update)
- **Pause button moved to bottom-left** (was defaulting to top-right).
  The "sit next to whatever's already stacked" collision logic now
  probes upward from that corner instead of downward, since downward
  from a bottom-left anchor would run off-screen.
- **Bot menu panel now opens centered** instead of bottom-left (per this
  request — reverses the last update's change).
- **Debug HUD rewritten** to remove its remaining dependency on
  `PlayLayer`-specific timing/parenting entirely:
  - Labels are now created lazily by `MacroManager` itself, the first
    time `updateHud()` runs with a scene available, and parented
    directly to the scene root — not to `PlayLayer` at all, and not
    handed off from `PlayLayer::init()` (which no longer does anything
    HUD-related).
  - Moved to the bottom-right corner, right-anchored, away from GD's own
    top-focused HUD elements (percentage/attempt labels) in case
    overlap was ever part of the problem.
  - Added proper `retain()`/`release()` around the labels' lifetime so a
    scene teardown that doesn't go through our own `clearHud()` (called
    from `PlayLayer::onQuit`) leaves a safely-orphaned node instead of a
    dangling pointer.
- **Investigated accuracy for ship/wave and other non-cube gamemodes.**
  Short version: capture/playback needed no gamemode-specific changes at
  all — GD funnels every holdable control (jump, ship thrust, wave
  direction, ball switch, UFO flap, robot jump, spider teleport, swing
  flip) through the same `handleButton` call already hooked generically.
  What genuinely differs: ship/wave are continuous, hold-sensitive
  controls where precise play needs many more, much shorter press/
  release pairs per second than cube ever does — real extra input
  transitions, not an inefficiency in storing them, and also why ship/
  wave sections have effectively zero tolerance for timing drift (a
  cube jump has some forgiveness; a continuous mode's position a moment
  later is a direct function of exactly when you let go).
  - Added a real size optimization for that denser pattern anyway:
    `.mbf` bumped to format v4, storing each event's frame as a
    **varint delta** from the previous event's frame (LEB128-style)
    instead of a fixed 4-byte absolute value, and packing `state`+
    `player` into one flags byte instead of two. Dense ship/wave input
    typically drops from 8 bytes/event to 3-4; sparse cube sections
    essentially never end up bigger either. Fully lossless — the
    in-memory `InputEvent`/`MacroData` structs are unchanged, only the
    on-disk byte layout is different.

## v1.1.0-beta.8 (major update — bot menu rebuilt)
- **Rebuilt the bot menu panel from scratch, no longer based on
  `geode::Popup`/`FLAlertLayer`.** The last attempt to fix its position
  (moving the whole `Popup` node to bottom-left) broke it badly: split
  card/content, unclosable, rendered off-screen. Root cause: `Popup` is
  internally a full-screen overlay, and its card elements
  (`m_bgSprite`/`m_mainLayer`/etc.) are positioned at window-center by
  code we don't control — moving the outer node doesn't move everything
  as one clean unit, and can desync its own touch/hit-testing.
- The panel is now a plain, self-contained `CCLayer` we build and
  position entirely ourselves: its own background (`CCScale9Sprite`),
  its own title, its own close button (top-right X), and its own
  `keyBackClicked()` override so the hardware/Android back button closes
  it too. Since it's one single node hierarchy, the background and the
  buttons can never end up in different places from each other again.
- Fixed at the **bottom-left** of the screen (10px margin) via a direct
  `setPosition()` on our own layer — no more fighting an inherited
  class's internal centering logic.
- `BotMenuPopup::create()->show()` still works the same from the pause
  button's callback; `closeIfOpen()` (used by `LoadPopup`) now closes via
  the same self-contained removal logic.

## v1.1.0-beta.7
- Fixed the build error from last time: `pl->updateDebugHud()` doesn't
  compile because a method added inside a `$modify(PlayLayer)` class
  isn't actually part of `PlayLayer`'s real interface as seen from a
  *different* `$modify` class (`PlayerObject`'s) — `PlayLayer` genuinely
  has no member by that name from the compiler's point of view. Moved
  the HUD refresh logic into `MacroManager` itself (`setHudLabels()` /
  `updateHud()`), which is a plain singleton reachable from anywhere, and
  have `PlayLayer::init()` hand it the two label pointers instead of
  keeping them local to that class. `PlayerObject::update` now calls
  `MacroManager::get().updateHud()` directly.
- Pause-menu bot button made **40% bigger** (scale `0.9` → `1.26`); the
  placement collision-avoidance radius was scaled up to match so it still
  keeps a sensible gap from neighboring buttons.

## v1.1.0-beta.6
- **Bot menu now opens fixed at the bottom-left** of the screen instead
  of the default centered popup position. Reasserted in `onEnter()`
  (not just once at creation) since the base `Popup` may otherwise
  recenter it after.
- **Fixed the debug HUD still showing nothing**, for real this time —
  two compounding bugs:
  - The refresh hook (`PlayLayer::update(float dt)`) turned out to have
    the exact same silent-failure issue `handleButton` had: confirmed
    against the full generated member list, `PlayLayer` doesn't declare
    `update` itself at all (only inherits it), so that hook never fired.
    Moved the HUD refresh onto `PlayerObject::update` instead — the hook
    already proven working, since it's what makes playback work.
  - The label parenting "fix" from last time (moving them to `UILayer`)
    was based on a wrong guess: `UILayer`'s own methods
    (`enableEditorMode`, `editorPlaytest`) show it's the level editor's
    UI layer, not the gameplay HUD, so it's most likely null during
    normal play — silently falling back to the old (allegedly broken)
    parent. Checked the actual generated `PlayLayer` field list instead:
    `m_percentageLabel`/`m_attemptLabel` are plain direct children of
    `PlayLayer` and stay fixed on screen throughout gameplay, which
    means `PlayLayer`'s coordinate space was never actually the problem.
    Labels are back to being direct children of `PlayLayer`, matching
    GD's own HUD elements.
- **Added spacing between entries in the Load Macro list** — rows were
  stacked with zero gap (each row's step exactly matched its own
  height); added a 10px gap between rows.

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
