#include "MacroFormat.hpp"
#include <cstring>

namespace macrobot {

template <typename T>
static void pushRaw(std::vector<uint8_t>& out, T value) {
    static_assert(std::is_trivially_copyable<T>::value, "must be POD");
    const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

template <typename T>
static bool readRaw(const std::vector<uint8_t>& in, size_t& cursor, T& out) {
    if (cursor + sizeof(T) > in.size()) return false;
    std::memcpy(&out, in.data() + cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
}

// LEB128-style variable-length unsigned integer: 7 data bits per byte,
// high bit set means "more bytes follow". Small values (the common case
// for a delta between two nearby input frames) cost 1 byte; only a delta
// past ~268M frames would ever need the full 5 bytes.
static void pushVarint(std::vector<uint8_t>& out, uint32_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<uint8_t>(value & 0x7F) | 0x80);
        value >>= 7;
    }
    out.push_back(static_cast<uint8_t>(value));
}

static bool readVarint(const std::vector<uint8_t>& in, size_t& cursor, uint32_t& out) {
    uint32_t result = 0;
    int shift = 0;
    while (true) {
        if (cursor >= in.size()) return false;
        if (shift > 28) return false; // corrupt/absurdly long varint
        uint8_t byte = in[cursor++];
        result |= static_cast<uint32_t>(byte & 0x7F) << shift;
        if (!(byte & 0x80)) break;
        shift += 7;
    }
    out = result;
    return true;
}

std::vector<uint8_t> serializeMacro(const MacroData& data) {
    std::vector<uint8_t> out;
    out.reserve(16 + data.levelName.size() + data.events.size() * 4);

    out.insert(out.end(), kMagic, kMagic + 4);
    pushRaw<uint16_t>(out, kFormatVersion);
    pushRaw<uint16_t>(out, 0); // reserved

    pushRaw<uint32_t>(out, static_cast<uint32_t>(data.levelName.size()));
    out.insert(out.end(), data.levelName.begin(), data.levelName.end());

    pushRaw<uint32_t>(out, data.totalFrames);
    pushRaw<uint32_t>(out, static_cast<uint32_t>(data.events.size()));

    uint32_t prevFrame = 0;
    for (const auto& ev : data.events) {
        uint32_t delta = ev.frame - prevFrame; // events are recorded in
                                                // non-decreasing frame
                                                // order, so this never
                                                // underflows
        pushVarint(out, delta);
        pushRaw<uint8_t>(out, ev.button);
        uint8_t flags = (ev.state ? 0x1 : 0) | (ev.player ? 0x2 : 0);
        pushRaw<uint8_t>(out, flags);
        prevFrame = ev.frame;
    }

    return out;
}

bool deserializeMacro(const std::vector<uint8_t>& bytes, MacroData& out) {
    size_t cursor = 0;
    if (bytes.size() < 4) return false;
    if (std::memcmp(bytes.data(), kMagic, 4) != 0) return false;
    cursor = 4;

    uint16_t version = 0, reserved = 0;
    if (!readRaw(bytes, cursor, version)) return false;
    if (!readRaw(bytes, cursor, reserved)) return false;
    if (version != kFormatVersion) return false;

    uint32_t nameLen = 0;
    if (!readRaw(bytes, cursor, nameLen)) return false;
    if (cursor + nameLen > bytes.size()) return false;
    std::string levelName(reinterpret_cast<const char*>(bytes.data() + cursor), nameLen);
    cursor += nameLen;

    uint32_t totalFrames = 0;
    if (!readRaw(bytes, cursor, totalFrames)) return false;

    uint32_t eventCount = 0;
    if (!readRaw(bytes, cursor, eventCount)) return false;

    MacroData result;
    result.levelName = std::move(levelName);
    result.totalFrames = totalFrames;
    result.events.reserve(eventCount);

    uint32_t prevFrame = 0;
    for (uint32_t i = 0; i < eventCount; i++) {
        uint32_t delta = 0;
        uint8_t button = 0, flags = 0;
        if (!readVarint(bytes, cursor, delta)) return false;
        if (!readRaw(bytes, cursor, button)) return false;
        if (!readRaw(bytes, cursor, flags)) return false;

        InputEvent ev;
        ev.frame = prevFrame + delta;
        ev.button = button;
        ev.state = (flags & 0x1) != 0;
        ev.player = (flags & 0x2) != 0;
        result.events.push_back(ev);

        prevFrame = ev.frame;
    }

    out = std::move(result);
    return true;
}

}
