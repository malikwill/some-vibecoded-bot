#include "MacroManager.hpp"
#include <Geode/Geode.hpp>
#include <fstream>

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
    m_frame = 0;
    m_buffer = MacroData{};
    m_buffer.levelName = m_levelName;
    log::info("MacroBot: recording started for level '{}'", m_levelName);
}

void MacroManager::onLevelReset() {
    if (m_mode == Mode::Recording) {
        // The attempt that was just running is what we captured — the
        // session is now "finished". Stop capturing further attempts and
        // wait for the user to explicitly Save.
        m_buffer.totalFrames = m_frame;
        m_mode = Mode::Standby;
        log::info("MacroBot: attempt finished ({} events, {} frames) — waiting for Save",
                   m_buffer.events.size(), m_frame);
    }
    if (m_mode == Mode::Playing) {
        m_playCursor = 0;
    }
    m_frame = 0;
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
    return true;
}

std::optional<std::string> MacroManager::armedMacroName() const {
    return m_armedDisplayName;
}

void MacroManager::startPlaying() {
    if (!m_armed.has_value()) return;
    m_mode = Mode::Playing;
    m_frame = 0;
    m_playCursor = 0;
}

void MacroManager::stopPlaying() {
    if (m_mode == Mode::Playing) {
        m_mode = Mode::Idle;
    }
    m_playCursor = 0;
}

void MacroManager::onPhysicsStep(const std::function<void(uint8_t button, bool player1, bool down)>& fireInput) {
    if (m_mode == Mode::Playing && m_armed.has_value()) {
        const auto& events = m_armed->events;
        while (m_playCursor < events.size() && events[m_playCursor].frame <= m_frame) {
            const auto& ev = events[m_playCursor];
            fireInput(ev.button, ev.player, ev.state);
            m_playCursor++;
        }
        if (m_playCursor >= events.size()) {
            // Reached the end of the macro; stop consuming further steps
            // but leave the level running normally.
            m_mode = Mode::Idle;
        }
    }

    m_frame++;
}

void MacroManager::recordInput(int button, bool player1, bool down) {
    // Only capture while actively Recording — this is a no-op while on
    // Standby (waiting for Save) or in any other mode, by design.
    if (m_mode != Mode::Recording) return;
    InputEvent ev;
    ev.frame = m_frame;
    ev.button = static_cast<uint8_t>(button);
    ev.player = player1;
    ev.state = down;
    m_buffer.events.push_back(ev);
}

void MacroManager::ensureHudLabels() {
    if (m_hudFrameLabel && m_hudEventsLabel) return;

    auto scene = CCDirector::sharedDirector()->getRunningScene();
    if (!scene) return;

    // Parented directly to the scene root, not to PlayLayer — removes
    // any dependency on PlayLayer's own coordinate space or init timing.
    // Bottom-right corner, right-anchored, well away from GD's own
    // top-focused HUD elements (percentage/attempt labels) so nothing
    // overlaps or gets drawn over.
    auto winSize = CCDirector::sharedDirector()->getWinSize();

    m_hudFrameLabel = CCLabelBMFont::create("Frame: 0", "chatFont.fnt");
    m_hudFrameLabel->setAnchorPoint({1.f, 0.f});
    m_hudFrameLabel->setScale(0.5f);
    m_hudFrameLabel->setID("macrobot-frame-label"_spr);
    m_hudFrameLabel->setPosition({winSize.width - 6.f, 40.f});
    m_hudFrameLabel->retain(); // survive a scene-level removeAllChildren
                               // without leaving us a dangling pointer;
                               // released again in clearHud()
    scene->addChild(m_hudFrameLabel, 20000);

    m_hudEventsLabel = CCLabelBMFont::create("Events: 0", "chatFont.fnt");
    m_hudEventsLabel->setAnchorPoint({1.f, 0.f});
    m_hudEventsLabel->setScale(0.5f);
    m_hudEventsLabel->setID("macrobot-events-label"_spr);
    m_hudEventsLabel->setPosition({winSize.width - 6.f, 24.f});
    m_hudEventsLabel->retain();
    scene->addChild(m_hudEventsLabel, 20000);
}

void MacroManager::clearHud() {
    if (m_hudFrameLabel) {
        m_hudFrameLabel->removeFromParentAndCleanup(true);
        m_hudFrameLabel->release();
        m_hudFrameLabel = nullptr;
    }
    if (m_hudEventsLabel) {
        m_hudEventsLabel->removeFromParentAndCleanup(true);
        m_hudEventsLabel->release();
        m_hudEventsLabel = nullptr;
    }
}

void MacroManager::updateHud() {
    ensureHudLabels();

    bool show = m_debugHudEnabled;

    if (m_hudFrameLabel) m_hudFrameLabel->setVisible(show);
    if (m_hudEventsLabel) m_hudEventsLabel->setVisible(show);
    if (!show) return;

    if (m_hudFrameLabel) {
        m_hudFrameLabel->setString(("Frame: " + std::to_string(m_frame)).c_str());
    }

    if (m_hudEventsLabel) {
        std::string eventsText;
        switch (m_mode) {
            case Mode::Recording:
                eventsText = "Events: " + std::to_string(recordedEventCount()) + " (recording)";
                break;
            case Mode::Standby:
                eventsText = "Events: " + std::to_string(recordedEventCount()) + " (standby, unsaved)";
                break;
            case Mode::Playing:
                eventsText = "Events: " + std::to_string(playedEventCount()) + "/"
                              + std::to_string(totalArmedEventCount()) + " (playing)";
                break;
            default:
                eventsText = "Events: " + std::to_string(totalArmedEventCount()) + " armed";
                break;
        }
        m_hudEventsLabel->setString(eventsText.c_str());
    }
}

}
