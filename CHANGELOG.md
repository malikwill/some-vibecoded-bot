# Changelog

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
