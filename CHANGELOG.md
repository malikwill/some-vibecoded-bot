# Changelog

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
