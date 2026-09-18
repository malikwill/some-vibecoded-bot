#pragma once
#include <cstdint>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// .mbf — MacroBot Format
//
// Design goal: ACCURACY OVER SIZE.
// Every input transition is stored against the exact physics-step index it
// happened on (see MacroManager: the step counter is incremented once per
// PlayLayer physics tick, which in GD is fixed-rate and independent of
// render framerate). This means:
//   - Playback is frame-perfect regardless of the playing device's FPS.
//   - No compression / delta-packing is used that could ever lose or
//     misplace a single frame. Files are small anyway (one record per
//     button transition, not per frame), so there's no real size cost to
//     keeping the format simple and unambiguous.
//
// Layout:
//   [ 4 bytes]  magic        "MBF2"
//   [ 2 bytes]  formatVersion (uint16, little-endian)   -> currently 1
//   [ 2 bytes]  reserved      (uint16) -> 0
//   [ 4 bytes]  levelNameLen  (uint32)
//   [ N bytes]  levelName     (UTF-8, not null terminated)
//   [ 8 bytes]  totalSteps    (uint64) - length of the recorded attempt
//   [ 4 bytes]  eventCount    (uint32)
//   -- eventCount * InputEventRaw --
//     [ 8 bytes] step   (uint64) physics-step index the event fired on
//     [ 1 byte ] button (uint8)  GD button id (1=jump, 2=left, 3=right, ...)
//     [ 1 byte ] player (uint8)  1 = player1, 0 = player2 (dual mode)
//     [ 1 byte ] down   (uint8)  1 = pressed, 0 = released
//     [ 1 byte ] pad    (uint8)  reserved, always 0
// ---------------------------------------------------------------------------

namespace macrobot {

constexpr char kMagic[4] = {'M', 'B', 'F', '2'};
constexpr uint16_t kFormatVersion = 1;

struct InputEvent {
    uint64_t step = 0;
    uint8_t button = 0;
    bool player1 = true;
    bool down = false;
};

struct MacroData {
    std::string levelName;
    uint64_t totalSteps = 0;
    std::vector<InputEvent> events;
};

// Serializes MacroData to raw bytes in the .mbf layout described above.
std::vector<uint8_t> serializeMacro(const MacroData& data);

// Parses raw bytes back into MacroData. Returns false (and leaves `out`
// untouched) if the buffer isn't a valid/recognized .mbf file.
bool deserializeMacro(const std::vector<uint8_t>& bytes, MacroData& out);

}
