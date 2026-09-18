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
    out.reserve(20 + data.levelName.size() + data.events.size() * 12);

    out.insert(out.end(), kMagic, kMagic + 4);
    pushRaw<uint16_t>(out, kFormatVersion);
    pushRaw<uint16_t>(out, 0); // reserved
    pushRaw<uint32_t>(out, data.ticksPerSecond);

    pushRaw<uint32_t>(out, static_cast<uint32_t>(data.levelName.size()));
    out.insert(out.end(), data.levelName.begin(), data.levelName.end());

    pushRaw<uint64_t>(out, data.totalSteps);
    pushRaw<uint32_t>(out, static_cast<uint32_t>(data.events.size()));

    for (const auto& ev : data.events) {
        pushRaw<uint64_t>(out, ev.step);
        pushRaw<uint8_t>(out, ev.button);
        pushRaw<uint8_t>(out, ev.player1 ? 1 : 0);
        pushRaw<uint8_t>(out, ev.down ? 1 : 0);
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

    uint32_t ticksPerSecond = 240;
    if (!readRaw(bytes, cursor, ticksPerSecond)) return false;

    uint32_t nameLen = 0;
    if (!readRaw(bytes, cursor, nameLen)) return false;
    if (cursor + nameLen > bytes.size()) return false;
    std::string levelName(reinterpret_cast<const char*>(bytes.data() + cursor), nameLen);
    cursor += nameLen;

    uint64_t totalSteps = 0;
    if (!readRaw(bytes, cursor, totalSteps)) return false;

    uint32_t eventCount = 0;
    if (!readRaw(bytes, cursor, eventCount)) return false;

    MacroData result;
    result.levelName = std::move(levelName);
    result.totalSteps = totalSteps;
    result.ticksPerSecond = ticksPerSecond;
    result.events.reserve(eventCount);

    for (uint32_t i = 0; i < eventCount; i++) {
        uint64_t step = 0;
        uint8_t button = 0, player1 = 0, down = 0, pad = 0;
        if (!readRaw(bytes, cursor, step)) return false;
        if (!readRaw(bytes, cursor, button)) return false;
        if (!readRaw(bytes, cursor, player1)) return false;
        if (!readRaw(bytes, cursor, down)) return false;
        if (!readRaw(bytes, cursor, pad)) return false;

        InputEvent ev;
        ev.step = step;
        ev.button = button;
        ev.player1 = (player1 != 0);
        ev.down = (down != 0);
        result.events.push_back(ev);
    }

    out = std::move(result);
    return true;
}

}
