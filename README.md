# MacroBot

A Geode mod for Geometry Dash that records and replays frame-perfect input
macros, built for **GD 2.2081 / Geode SDK 5.10.1 / Android x64**.

## What it does

- A circular button is added to the **pause menu's bottom-left**
  corner (stacking upward next to whatever else is already there rather
  than overlapping it). Tapping it opens the bot menu, **centered** on
  screen: **Record**, **Save**, **Play**, **Load**.
- **Record** — automatically enters practice mode and resumes gameplay.
  Every button press/release is captured against the exact physics
  substep it happened on (via `PlayerObject::update`, which fires once
  per real GD physics tick — not once per rendered frame), so playback
  matches actual gameplay physics regardless of framerate. The moment
  that attempt ends (the level resets), capturing stops — further
  attempts are **not** recorded until you act.
- **Save** — only enabled once an attempt has been captured and finished.
  Writes it to disk under the level's name and arms it for Play. Tapping
  Record again instead discards the pending capture and starts fresh.
- **Play** — replays the currently loaded/saved macro from the start of
  the attempt.
- **Load** — opens a picker listing every saved macro (spaced out, not
  stuck together). Choosing one loads it and immediately starts playing
  it (auto-clicks Play for you).
- **Debug HUD** — a checkbox in the bot menu toggles a small on-screen
  overlay, bottom-right of the level (parented to the scene root, not
  PlayLayer, and created lazily on first use), showing the live frame
  counter and an event counter (recorded count while Recording,
  played/total while Playing, armed count otherwise), so what
  recording/playback is doing is actually visible instead of a black box.

## Macro format

Macros are saved as `.mbf` (**MacroBot Format**) files in the mod's save
folder (`.../geode/mods/<mod-id>/macros/`). Each input transition is
stored as a plain `{ frame, button, state, player }` record — `frame` is
the physics-substep index it happened on (a plain incrementing counter
driven by `PlayerObject::update`, which fires once per real GD physics
tick), `button` is the GD button id, `state` is pressed/released, and
`player` is player1/player2. See `src/MacroFormat.hpp` for the exact
byte layout.

This mirrors the core approach real `.gdr`/`.gdr2`-style macro tools use:
key every input to an actual physics-tick reference rather than to
render-frame count. An earlier revision of this mod counted
`PlayLayer::update()` calls instead — which fire once per *rendered*
frame — and that mismatch (GD runs several physics substeps inside a
single rendered frame) was the real reason playback didn't work at all.

**Ship/wave and other non-cube gamemodes** needed no special handling:
GD funnels every holdable control (cube jump, ship thrust, wave
direction, ball switch, UFO flap, robot jump, spider teleport, swing
flip) through the same `handleButton` call this mod already hooks
generically, so they're captured with the same mechanism and the same
per-substep accuracy as cube. What genuinely differs is that ship/wave
are continuous, hold-sensitive controls — precise play needs many more,
much shorter press/release pairs per second than cube ever does, which
is real extra input, not a storage inefficiency, and it's also why
those sections have effectively zero tolerance for a single tick of
timing drift (a cube jump has some forgiveness; a continuous mode's
position a moment later is a direct function of exactly when you let
go). On format v4, each event's `frame` is stored as a **varint delta**
from the previous event's frame instead of a fixed 4-byte value, and
`state`+`player` are packed into one flags byte — dense ship/wave input
typically drops from 8 bytes/event to 3-4, fully losslessly.

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

`BotMenuPopup` (`src/BotMenu.hpp/.cpp`) is a plain `CCLayer`, not
`geode::Popup`/`FLAlertLayer`. An earlier revision tried to reposition a
`Popup` to a fixed bottom-left spot by moving the whole node in
`onEnter()`; `Popup` is internally a full-screen overlay whose card
elements are positioned at window-center by code outside our control, so
that broke badly (split background/content, unclosable, rendered
off-screen). Building the panel ourselves — its own background, title,
close button, and `keyBackClicked()` override — means position and
closing are things we directly own. `LoadPopup` is unaffected and still
uses `geode::Popup` normally (centered, no custom positioning needed).

Input capture/injection is hooked on **`GJBaseGameLayer::handleButton`**,
not `PlayLayer::handleButton` — verified against a known-working
reference macro bot. GD's real `handleButton` virtual lives on
`GJBaseGameLayer`; `PlayLayer` inherits it but doesn't redeclare it, so a
hook placed on `PlayLayer` compiles fine but never actually intercepts
real input. That silent mismatch was the main reason nothing worked.

Frame-accurate timing is hooked on **`PlayerObject::update(float
stepDelta)`**. GD runs several fixed physics substeps inside a single
rendered frame, and `PlayerObject::update` is what fires once per real
substep (verified against the reference bot) — a much finer grain than
a rendered-frame hook could give. It's guarded to `this == pl->m_player1`
so 2-player/dual mode doesn't double-count. The debug HUD's refresh is
also piggybacked on this same hook, rather than a separate
`PlayLayer::update(float dt)` hook: checked against the full generated
`PlayLayer` member list, `PlayLayer` doesn't declare `update` at all —
only inherits it — which is the same "declared on an ancestor, so a
`$modify(PlayLayer)` hook silently never fires" issue `handleButton` had.
The `PlayerObject::update` hook is already proven working (it's what
makes playback work at all), so it's the safe place to drive the HUD too,
via a public `updateDebugHud()` method added to `PlayLayer`.

The debug HUD's labels are created lazily by `MacroManager` itself
(`ensureHudLabels()`, called from `updateHud()`) and parented directly to
the scene root — not to `PlayLayer` and not handed off from
`PlayLayer::init()` at all anymore. Earlier revisions tried parenting to
`PlayLayer` directly, then to `UILayer` (wrong guess — `UILayer`'s own
methods like `enableEditorMode`/`editorPlaytest` show it's the level
editor's UI layer, not the gameplay HUD, so it's most likely null during
normal play), then back to `PlayLayer` again with the refresh routed
through `MacroManager` for cross-`$modify`-class reasons (see below).
Parenting to the scene root sidesteps needing to reason about any of
`PlayLayer`'s specifics at all. The labels are `retain()`'d while held
here and `release()`'d in `clearHud()` (called from `PlayLayer::onQuit`)
so a scene teardown that doesn't go through that path leaves a safely
orphaned node rather than a dangling pointer.

`src/main.cpp` also still hooks `PlayLayer::resetLevel`, `PlayLayer::onQuit`,
and `PauseLayer::customSetup` — these ARE declared directly on their
respective classes (confirmed against the generated bindings), so they
don't have the same silent-hook risk as `handleButton`/`update` did. If
`geode build` ever reports a mismatch for 2.2081 on any hook here, open
the generated bindings for the relevant class in your local Geode SDK
checkout and check both the signature AND which class actually declares
it before adjusting; none of the recording/playback logic in
`MacroManager` needs to change either way.

The pause-menu button placement does **not** rely on any specific
`PauseLayer` member or node ID (an earlier revision assumed an
`m_buttonMenu` field that doesn't actually exist in the generated
bindings — confirmed against the docs, `PauseLayer` only exposes
`m_unfocused`/`m_tryingQuit` as fields). `customSetup()` defers actual
placement one frame via `scheduleOnce`, so it runs after every other
mod's `customSetup()` (all synchronous within the same init call) has
already added its buttons — avoiding a same-frame ordering race where a
later-running mod's button wasn't visible yet when we scanned. It then
recursively walks every `CCMenuItem` near our default corner, finds the
lowest edge of that cluster, and sits directly below it.

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
