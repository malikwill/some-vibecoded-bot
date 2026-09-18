# Changelog

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
