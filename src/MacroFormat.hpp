#pragma once
#include <cstdint>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// .mbf — MacroBot Format
//
// Design goal: ACCURACY OVER SIZE, using the same underlying approach as
// real GD replay/macro formats (.gdr2-style): every input transition is
// keyed to a FIXED-RATE VIRTUAL CLOCK TICK, not to "however many times
// PlayLayer::update() happened to fire." update() runs once per rendered
// frame, and frame timing varies with the device's real framerate — so a
// recording session and a playback session never produce the same
// sequence of update() calls, even on the same device. A fixed tick rate
// (see MacroManager::kTicksPerSecond, currently 240 ticks/sec, advanced
// by accumulating real dt) gives both sides an identical, FPS-independent
// timeline, which is what actually makes playback frame-perfect.
//
// Layout:
//   [ 4 bytes]  magic         "MBF2"
//   [ 2 bytes]  formatVersion (uint16, little-endian)   -> currently 2
//   [ 2 bytes]  reserved      (uint16) -> 0
//   [ 4 bytes]  ticksPerSecond (uint32) -> the virtual clock rate every
//                                          `step` below was recorded
//                                          against (see MacroManager)
//   [ 4 bytes]  levelNameLen  (uint32)
//   [ N bytes]  levelName     (UTF-8, not null terminated)
//   [ 8 bytes]  totalSteps    (uint64) - length of the recorded attempt,
//                                        in ticks
//   [ 4 bytes]  eventCount    (uint32)
//   -- eventCount * InputEventRaw --
//     [ 8 bytes] step   (uint64) tick index the event fired on
//     [ 1 byte ] button (uint8)  GD button id (1=jump, 2=left, 3=right, ...)
//     [ 1 byte ] player (uint8)  1 = player1, 0 = player2 (dual mode)
//     [ 1 byte ] down   (uint8)  1 = pressed, 0 = released
//     [ 1 byte ] pad    (uint8)  reserved, always 0
// ---------------------------------------------------------------------------

namespace macrobot {

constexpr char kMagic[4] = {'M', 'B', 'F', '2'};
constexpr uint16_t kFormatVersion = 2;

struct InputEvent {
    uint64_t step = 0;
    uint8_t button = 0;
    bool player1 = true;
    bool down = false;
};

struct MacroData {
    std::string levelName;
    uint64_t totalSteps = 0;
    uint32_t ticksPerSecond = 240; // set from MacroManager::kTicksPerSecond on save
    std::vector<InputEvent> events;
};

// Serializes MacroData to raw bytes in the .mbf layout described above.
std::vector<uint8_t> serializeMacro(const MacroData& data);

// Parses raw bytes back into MacroData. Returns false (and leaves `out`
// untouched) if the buffer isn't a valid/recognized .mbf file.
bool deserializeMacro(const std::vector<uint8_t>& bytes, MacroData& out);

}
