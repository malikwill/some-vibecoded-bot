# MacroBot

A Geode mod for Geometry Dash that records and replays frame-perfect input
macros, built for **GD 2.2081 / Geode SDK 5.10.1 / Android x64**.

## What it does

- A circular button is added to the **pause menu**, in its rightful spot
  in the existing button row (not overlapping whatever GD already has
  there). Tapping it opens the bot menu: **Record**, **Save**, **Play**,
  **Load**.
- **Record** — automatically enters practice mode and resumes gameplay.
  Every button press/release is captured against a fixed-rate virtual
  clock tick (240 ticks/sec), not wall-clock time or render-frame count,
  so playback is frame-perfect regardless of the device's framerate. The
  moment that attempt ends (the level resets), capturing stops — further
  attempts are **not** recorded until you act.
- **Save** — only enabled once an attempt has been captured and finished.
  Writes it to disk under the level's name and arms it for Play. Tapping
  Record again instead discards the pending capture and starts fresh.
- **Play** — replays the currently loaded/saved macro from the start of
  the attempt.
- **Load** — opens a picker listing every saved macro. Choosing one
  loads it and immediately starts playing it (auto-clicks Play for you).
- **Debug HUD** — a checkbox in the bot menu toggles a small on-screen
  overlay (top-left of the level) showing the live tick counter and an
  event counter (recorded count while Recording, played/total while
  Playing, armed count otherwise), so what recording/playback is doing
  is actually visible instead of a black box.

## Macro format

Macros are saved as `.mbf` (**MacroBot Format**) files in the mod's save
folder (`.../geode/mods/<mod-id>/macros/`). The format intentionally
favors **accuracy over file size**, using the same underlying approach as
real `.gdr`/`.gdr2`-style replay files: every input transition is stored
as `(tick, button, player, pressed)`, where `tick` is an index into a
**fixed-rate virtual clock** (`MacroManager::kTicksPerSecond`, currently
240/sec) — not a count of how many times `PlayLayer::update()` happened
to fire. `update()` runs once per rendered frame, so its call frequency
varies with the device's real framerate; keying recorded steps to that
was the actual bug that made playback not work at all, since a recording
session and a playback session never produce identical `update()` timing.
The fixed tick rate is accumulated from real `dt` each frame (see
`MacroManager::onPhysicsStep`), giving both sides the same FPS-independent
timeline. The file header also embeds the tick rate it was recorded at,
for forward-compatibility — see `src/MacroFormat.hpp` for the exact byte
layout.

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
   or build via the included GitHub Actions workflow
   (`.github/workflows/build.yml`) — every push builds Android64 and
   uploads the `.geode` package as a workflow artifact.

## A note on bindings

`src/main.cpp` hooks `PlayLayer::update`, `PlayLayer::handleButton`,
`PlayLayer::resetLevel`, `PlayLayer::onQuit`, and `PauseLayer::customSetup`.
These match the commonly-used signatures across recent GD versions, but
Geode's generated bindings are version-specific. If `geode build` reports
a signature mismatch for 2.2081, open the generated bindings for
`PlayLayer` / `GJBaseGameLayer` in your local Geode SDK checkout and
adjust the hook signatures in `main.cpp` to match exactly — none of the
recording/playback logic in `MacroManager` needs to change.

The pause-menu button placement does **not** rely on any specific
`PauseLayer` member or node ID (an earlier revision assumed an
`m_buttonMenu` field that doesn't actually exist in the generated
bindings — confirmed against the docs, `PauseLayer` only exposes
`m_unfocused`/`m_tryingQuit` as fields). Instead, `customSetup()`
recursively walks every `CCMenuItem` already in the pause layer, computes
their on-screen rects, and places our button at the first free slot going
down the right edge — genuine collision detection rather than a hardcoded
or guessed position, which is what actually avoids it landing on top of
an existing button.

`src/BotMenu.cpp`'s Record button also calls `PlayLayer::m_isPracticeMode`
and `PauseLayer::onPracticeMode(CCObject*)` to auto-enter practice mode
(the same function GD's own practice-mode pause button calls — confirmed
against the generated bindings). If `m_isPracticeMode` doesn't match your
bindings exactly, check the generated `PlayLayer` member list for the
practice-mode flag and update `enterPracticeAndResume()` in
`BotMenu.cpp` accordingly.

## Recording model

- **Record** discards anything previously captured/pending and starts a
  fresh capture, auto-entering practice mode.
- The session is considered "finished" the moment the level resets
  (`PlayLayer::resetLevel`) — practically, whenever the attempt restarts.
  At that point the mod stops capturing further input (moves to
  **Standby**) but keeps what was captured; nothing further is recorded
  until you act.
- **Save** (only enabled on Standby) writes the captured attempt to disk
  and arms it for Play. Tapping **Record** again instead of Save discards
  the standby buffer and starts over.
- Leaving the level without saving (Standby or still Recording) discards
  the buffer — there is no auto-save.

## Versioning

- Regular updates bump the mod version as a **beta** release
  (`vX.Y.Z-beta.N`) and update `CHANGELOG.md`.
- Major updates also get a refreshed `README.md`.
- Each update ships only the files that changed — the full project zip
  is only provided once, on first delivery.
