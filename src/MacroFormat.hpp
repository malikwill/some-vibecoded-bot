#pragma once
#include <cstdint>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// .mbf — MacroBot Format
//
// Simple, explicit event layout — one entry per input transition:
//   { frame, button, state, player }
// where `frame` is a plain incrementing counter of PlayLayer::update()
// calls (i.e. rendered frames) during the original recording session,
// `state` is pressed(true)/released(false), and `player` is
// player1(true)/player2(false). This matches how classic .gdr/.gdr2-style
// GD macro files key their events.
//
// Layout:
//   [ 4 bytes]  magic         "MBF2"
//   [ 2 bytes]  formatVersion (uint16, little-endian)   -> currently 3
//   [ 2 bytes]  reserved      (uint16) -> 0
//   [ 4 bytes]  levelNameLen  (uint32)
//   [ N bytes]  levelName     (UTF-8, not null terminated)
//   [ 4 bytes]  totalFrames   (uint32) - length of the recorded attempt
//   [ 4 bytes]  eventCount    (uint32)
//   -- eventCount * InputEventRaw (8 bytes each) --
//     [ 4 bytes] frame  (uint32) frame index the event fired on
//     [ 1 byte ] button (uint8)  GD button id (1=jump, 2=left, 3=right, ...)
//     [ 1 byte ] state  (uint8)  1 = pressed, 0 = released
//     [ 1 byte ] player (uint8)  1 = player1, 0 = player2 (dual mode)
//     [ 1 byte ] pad    (uint8)  reserved, always 0
// ---------------------------------------------------------------------------

namespace macrobot {

constexpr char kMagic[4] = {'M', 'B', 'F', '2'};
constexpr uint16_t kFormatVersion = 3;

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
