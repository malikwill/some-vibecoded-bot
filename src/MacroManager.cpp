#include "MacroManager.hpp"
#include <Geode/Geode.hpp>
#include <fstream>
#include <chrono>
#include <ctime>
#include <sstream>

using namespace geode::prelude;

namespace macrobot {

MacroManager& MacroManager::get() {
    static MacroManager instance;
    return instance;
}

std::filesystem::path MacroManager::macrosDir() const {
    auto dir = Mod::get()->getSaveDir() / "macros";
    if (!std::filesystem::exists(dir)) {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }
    return dir;
}

static std::string sanitizeFilename(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' || c == '-' || c == '_') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    if (out.empty()) out = "macro";
    return out;
}

void MacroManager::startRecording() {
    m_mode = Mode::Recording;
    m_step = 0;
    m_accumulator = 0.0;
    m_buffer = MacroData{};
    m_buffer.levelName = m_levelName;
    m_buffer.ticksPerSecond = static_cast<uint32_t>(kTicksPerSecond);
    log::info("MacroBot: recording started for level '{}'", m_levelName);
}

void MacroManager::onLevelReset() {
    if (m_mode == Mode::Recording) {
        // The attempt that was just running is what we captured — the
        // session is now "finished". Stop capturing further attempts and
        // wait for the user to explicitly Save.
        m_buffer.totalSteps = m_step;
        m_mode = Mode::Standby;
        log::info("MacroBot: attempt finished ({} events, {} ticks) — waiting for Save",
                   m_buffer.events.size(), m_step);
    }
    if (m_mode == Mode::Playing) {
        m_playCursor = 0;
    }
    m_step = 0;
    m_accumulator = 0.0;
}

void MacroManager::saveStandbyMacro() {
    if (m_mode != Mode::Standby) return;

    auto base = sanitizeFilename(m_buffer.levelName.empty() ? "macro" : m_buffer.levelName);
    auto dir = macrosDir();
    std::filesystem::path path = dir / (base + ".mbf");

    // Don't clobber an existing macro with the same level name — append a
    // numeric suffix instead.
    int suffix = 2;
    while (std::filesystem::exists(path)) {
        path = dir / (base + " (" + std::to_string(suffix) + ").mbf");
        suffix++;
    }

    auto bytes = serializeMacro(m_buffer);
    std::ofstream f(path, std::ios::binary);
    if (f) {
        f.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        log::info("MacroBot: saved macro to {}", path.string());
    } else {
        log::error("MacroBot: failed to write macro file {}", path.string());
        return;
    }

    // Arm what we just saved so hitting Play immediately after Save just
    // works without needing to reload it from disk.
    m_armed = m_buffer;
    m_armedDisplayName = m_buffer.levelName;

    m_mode = Mode::Idle;
    m_buffer = MacroData{};
}

void MacroManager::cancelRecording() {
    m_mode = Mode::Idle;
    m_buffer = MacroData{};
}

bool MacroManager::loadMacroFromFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        log::error("MacroBot: could not open {}", path.string());
        return false;
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    MacroData data;
    if (!deserializeMacro(bytes, data)) {
        log::error("MacroBot: {} is not a valid .mbf file", path.string());
        return false;
    }

    m_armed = data;
    m_armedDisplayName = path.stem().string();
    log::info("MacroBot: loaded macro '{}' ({} events)", m_armedDisplayName.value(), data.events.size());
    if (data.ticksPerSecond != static_cast<uint32_t>(kTicksPerSecond)) {
        log::warn("MacroBot: '{}' was recorded at {} ticks/sec but this build plays back at {} — timing will be off",
                   m_armedDisplayName.value(), data.ticksPerSecond, static_cast<uint32_t>(kTicksPerSecond));
    }
    return true;
}

std::optional<std::string> MacroManager::armedMacroName() const {
    return m_armedDisplayName;
}

void MacroManager::startPlaying() {
    if (!m_armed.has_value()) return;
    m_mode = Mode::Playing;
    m_step = 0;
    m_accumulator = 0.0;
    m_playCursor = 0;
}

void MacroManager::stopPlaying() {
    if (m_mode == Mode::Playing) {
        m_mode = Mode::Idle;
    }
    m_playCursor = 0;
}

void MacroManager::onPhysicsStep(float dt, const std::function<void(uint8_t button, bool player1, bool down)>& fireInput) {
    // Guard against a huge dt (e.g. the game was backgrounded) turning
    // into thousands of ticks in one call.
    if (dt > 0.25f) dt = 0.25f;

    m_accumulator += dt;

    while (m_accumulator >= kSecondsPerTick) {
        m_accumulator -= kSecondsPerTick;

        if (m_mode == Mode::Playing && m_armed.has_value()) {
            const auto& events = m_armed->events;
            while (m_playCursor < events.size() && events[m_playCursor].step <= m_step) {
                const auto& ev = events[m_playCursor];
                fireInput(ev.button, ev.player1, ev.down);
                m_playCursor++;
            }
            if (m_playCursor >= events.size()) {
                // Reached the end of the macro; stop consuming further
                // ticks but leave the level running normally.
                m_mode = Mode::Idle;
            }
        }

        m_step++;
    }
}

void MacroManager::recordInput(int button, bool player1, bool down) {
    // Only capture while actively Recording — this is a no-op while on
    // Standby (waiting for Save) or in any other mode, by design.
    if (m_mode != Mode::Recording) return;
    InputEvent ev;
    ev.step = m_step;
    ev.button = static_cast<uint8_t>(button);
    ev.player1 = player1;
    ev.down = down;
    m_buffer.events.push_back(ev);
}

}
