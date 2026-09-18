# MacroBot

A Geode mod for Geometry Dash that records and replays frame-perfect input
macros, built for **GD 2.2081 / Geode SDK 5.10.1 / Android x64**.

## What it does

- A circular button is added to the **pause menu**. Tapping it opens the
  bot menu with three controls: **Record**, **Play**, **Load**.
- **Record** — press it, then enter practice mode. Every button
  press/release is captured against the exact physics step it happened
  on (not wall-clock time), so playback is frame-perfect regardless of
  the device's framerate. When you finish the practice session (leave
  the level), the macro is **auto-saved** under the level's name.
- **Play** — replays the currently loaded/just-recorded macro from the
  start of the attempt.
- **Load** — opens a picker listing every saved macro. Choosing one
  loads it and immediately starts playing it (auto-clicks Play for you).

## Macro format

Macros are saved as `.mbf` (**MacroBot Format**) files in the mod's save
folder (`.../geode/mods/<mod-id>/macros/`). The format intentionally
favors **accuracy over file size**: every input transition is stored as
`(physics step, button, player, pressed)`. It reuses the same "record
discrete transitions rather than continuous per-frame state" approach as
`.gdr2`-style macro files, but with its own simple, fully-documented
binary layout — see `src/MacroFormat.hpp` for the exact byte layout.

## Project layout

```
MacroBot/
├── mod.json                 # Geode mod manifest
├── CMakeLists.txt
├── resources/
│   ├── logo.png              # mod listing icon
│   └── botButton.png         # circular pause-menu button sprite
├── src/
│   ├── MacroFormat.hpp/.cpp  # .mbf binary read/write
│   ├── MacroManager.hpp/.cpp # recording/playback state machine + file IO
│   ├── BotMenu.hpp/.cpp      # Record/Play/Load popup
│   ├── LoadPopup.hpp/.cpp    # saved-macro picker popup
│   └── main.cpp              # PlayLayer + PauseLayer hooks
└── .github/workflows/build.yml
```

## Building

1. Install the [Geode CLI](https://docs.geode-sdk.org/getting-started/) and
   set the `GEODE_SDK` environment variable to your Geode SDK checkout.
2. From the project root:
   ```
   geode build
   ```
   or build cross-platform releases automatically via the included
   GitHub Actions workflow (`.github/workflows/build.yml`) — every push
   builds Android64/Windows/macOS and combines them into one `.geode`
   file as a workflow artifact.

## A note on bindings

`src/main.cpp` hooks `PlayLayer::update`, `PlayLayer::handleButton`,
`PlayLayer::resetLevel`, `PlayLayer::onQuit`, and `PauseLayer::customSetup`.
These match the commonly-used signatures across recent GD versions, but
Geode's generated bindings are version-specific. If `geode build` reports
a signature mismatch for 2.2081, open the generated bindings for
`PlayLayer` / `GJBaseGameLayer` in your local Geode SDK checkout and
adjust the hook signatures in `main.cpp` to match exactly — none of the
recording/playback logic in `MacroManager` needs to change.

## Versioning

- Regular updates bump the mod version as a **beta** release
  (`vX.Y.Z-beta.N`) and update `CHANGELOG.md`.
- Major updates also get a refreshed `README.md`.
- Each update ships only the files that changed — the full project zip
  is only provided once, on first delivery.
