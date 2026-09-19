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

std::vector<uint8_t> serializeMacro(const MacroData& data) {
    std::vector<uint8_t> out;
    out.reserve(16 + data.levelName.size() + data.events.size() * 8);

    out.insert(out.end(), kMagic, kMagic + 4);
    pushRaw<uint16_t>(out, kFormatVersion);
    pushRaw<uint16_t>(out, 0); // reserved

    pushRaw<uint32_t>(out, static_cast<uint32_t>(data.levelName.size()));
    out.insert(out.end(), data.levelName.begin(), data.levelName.end());

    pushRaw<uint32_t>(out, data.totalFrames);
    pushRaw<uint32_t>(out, static_cast<uint32_t>(data.events.size()));

    for (const auto& ev : data.events) {
        pushRaw<uint32_t>(out, ev.frame);
        pushRaw<uint8_t>(out, ev.button);
        pushRaw<uint8_t>(out, ev.state ? 1 : 0);
        pushRaw<uint8_t>(out, ev.player ? 1 : 0);
        pushRaw<uint8_t>(out, 0); // pad
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

    for (uint32_t i = 0; i < eventCount; i++) {
        uint32_t frame = 0;
        uint8_t button = 0, state = 0, player = 0, pad = 0;
        if (!readRaw(bytes, cursor, frame)) return false;
        if (!readRaw(bytes, cursor, button)) return false;
        if (!readRaw(bytes, cursor, state)) return false;
        if (!readRaw(bytes, cursor, player)) return false;
        if (!readRaw(bytes, cursor, pad)) return false;

        InputEvent ev;
        ev.frame = frame;
        ev.button = button;
        ev.state = (state != 0);
        ev.player = (player != 0);
        result.events.push_back(ev);
    }

    out = std::move(result);
    return true;
}

}
