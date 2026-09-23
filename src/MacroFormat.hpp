#pragma once
#include <cstdint>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// .mbf — MacroBot Format
//
// One entry per input transition: { frame, button, state, player }, where
// `frame` is the physics-substep index it happened on (see MacroManager,
// driven by PlayerObject::update — a real, fixed-rate physics tick, not a
// rendered-frame count), `state` is pressed(true)/released(false), and
// `player` is player1(true)/player2(false).
//
// WHY GAMEMODE DOESN'T CHANGE THE CAPTURE LOGIC:
// GD funnels every holdable control (cube jump, ship thrust, wave
// direction, ball switch, UFO flap, robot jump, spider teleport, swing
// gravity flip) through the SAME handleButton(down, button, player2)
// call — there's no separate "ship input" or "wave input" entry point.
// Recording/playback here hooks that one function generically (see
// main.cpp's GJBaseGameLayer::handleButton hook) without ever special-
// casing a gamemode, so ship/wave/etc. are captured with exactly the
// same mechanism and exactly the same per-substep timing accuracy as
// cube — nothing extra was needed for correctness there.
//
// WHAT genuinely DOES differ for ship/wave, and why files get bigger:
// Cube's jump matters mostly at the instant of the press (a discrete
// event); ship and wave are direct, continuous controls where the
// player's trajectory is a function of "is the button held" at every
// single tick. Precise ship/wave play involves many more, much shorter
// press/release pairs per second than cube ever needs — i.e. genuinely
// more INPUT TRANSITIONS for the same span of gameplay, not any
// inefficiency in how each one is stored. That also means ship/wave
// sections have effectively zero tolerance for a single tick of timing
// drift (a cube jump has some forgiveness; a ship's vertical position a
// moment later is a continuous function of exactly when you let go) —
// they're the most sensitive stress-test for this mod's timing accuracy,
// not a special case that needs separate handling.
//
// FORMAT v4 — size optimization for that denser input pattern:
// Each event's `frame` is now stored as a VARINT DELTA from the previous
// event's frame (LEB128-style: 7 data bits per byte, high bit = "more
// bytes follow"), and `state`+`player` are packed into a single flags
// byte instead of two. For a dense run of ship/wave taps (small deltas
// between consecutive frames), this typically drops each event from a
// fixed 8 bytes to 3-4 bytes — a real, fully lossless size reduction
// exactly where "much more file size by input" actually comes from.
// Sparser cube sections (large deltas between rare jumps) don't shrink
// as much, but essentially never end up bigger than the old fixed
// layout either (a delta needs ~5 bytes only past a ~268M-frame gap,
// which is not a realistic single gap between two inputs).
//
// Layout:
//   [ 4 bytes]  magic         "MBF2"
//   [ 2 bytes]  formatVersion (uint16, little-endian)   -> currently 4
//   [ 2 bytes]  reserved      (uint16) -> 0
//   [ 4 bytes]  levelNameLen  (uint32)
//   [ N bytes]  levelName     (UTF-8, not null terminated)
//   [ 4 bytes]  totalFrames   (uint32) - length of the recorded attempt
//   [ 4 bytes]  eventCount    (uint32)
//   -- eventCount * variable-length records --
//     [varint]   frameDelta (uint32) frame index minus the previous
//                event's frame (first event's delta is from frame 0)
//     [ 1 byte ] button     (uint8) GD button id (1=jump, 2=left, 3=right, ...)
//     [ 1 byte ] flags      (uint8) bit0 = state (1=pressed), bit1 = player
//                                   (1=player1)
// ---------------------------------------------------------------------------

namespace macrobot {

constexpr char kMagic[4] = {'M', 'B', 'F', '2'};
constexpr uint16_t kFormatVersion = 4;

struct InputEvent {
    uint32_t frame = 0;
    uint8_t button = 0;
    bool state = false;   // pressed / released
    bool player = true;   // player1 / player2
};

struct MacroData {
    std::string levelName;
    uint32_t totalFrames = 0;
    std::vector<InputEvent> events;
};

// Serializes MacroData to raw bytes in the .mbf layout described above.
std::vector<uint8_t> serializeMacro(const MacroData& data);

// Parses raw bytes back into MacroData. Returns false (and leaves `out`
// untouched) if the buffer isn't a valid/recognized .mbf file.
bool deserializeMacro(const std::vector<uint8_t>& bytes, MacroData& out);

}
