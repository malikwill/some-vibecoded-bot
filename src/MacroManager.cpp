#include "MacroManager.hpp"
#include <Geode/Geode.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <map>

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

// Removes, for each (button, player) combo, its final event IF that
// final event is a press — i.e. a button still "held" when recording
// stopped. GD's hold-jump-to-restart gesture (or a jump simply cut short
// by death) generates a real handleButton(true, ...) call indistinguishable
// from any other press at the moment it happens, but it isn't meaningful
// gameplay input worth keeping in the saved macro.
static void trimTrailingUnreleasedPresses(std::vector<InputEvent>& events) {
    std::map<std::pair<uint8_t, bool>, size_t> lastIndex;
    for (size_t i = 0; i < events.size(); i++) {
        lastIndex[{events[i].button, events[i].player}] = i;
    }

    std::vector<size_t> toRemove;
    for (auto& [key, idx] : lastIndex) {
        if (events[idx].state) {
            toRemove.push_back(idx);
        }
    }
    if (toRemove.empty()) return;

    std::sort(toRemove.begin(), toRemove.end());
    for (auto it = toRemove.rbegin(); it != toRemove.rend(); ++it) {
        events.erase(events.begin() + static_cast<long>(*it));
    }
}

// Drops trailing events that landed on essentially the same tick as the
// reset itself. On death, GD force-releases any button the player was
// still holding as part of its own cleanup — that generates a real
// handleButton(false, ...) call, indistinguishable at the moment it
// happens from a deliberate release, but it's death cleanup, not
// something the player actually did. Unlike an unreleased trailing
// press, this looks like a perfectly normal, properly-paired release —
// so it isn't caught by trimTrailingUnreleasedPresses above and needs
// its own pass. A tiny tolerance (a couple of frames) is enough to catch
// it without discarding a genuine release the player happened to make
// just before dying.
static void trimTrailingEventsAtFrame(std::vector<InputEvent>& events, uint32_t resetFrame) {
    while (!events.empty() && events.back().frame + 2 >= resetFrame) {
        events.pop_back();
    }
}

void MacroManager::startRecording() {
    m_mode = Mode::Recording;
    m_frame = 0;
    m_buffer = MacroData{};
    m_buffer.levelName = m_levelName;
    m_positionLog.clear();
    m_haveStartX = false;
    m_logThrottle = 0;
    log::info("MacroBot: recording started for level '{}'", m_levelName);
}

void MacroManager::logPosition(float x) {
    if (m_mode != Mode::Recording) return;

    if (!m_haveStartX) {
        m_sessionStartX = x;
        m_haveStartX = true;
    }

    // Throttle: a sample every few substeps is already far more
    // resolution than matching a respawn position back to a frame
    // needs, and keeps this cheap over a long recording.
    if (++m_logThrottle < 4) return;
    m_logThrottle = 0;

    // Keep the log monotonic (skip samples that don't advance past the
    // last one) and bounded, so an extremely long recording can't grow
    // this unboundedly.
    if (!m_positionLog.empty() && x <= m_positionLog.back().first) return;
    m_positionLog.emplace_back(x, m_frame);
    if (m_positionLog.size() > 20000) {
        m_positionLog.erase(m_positionLog.begin(), m_positionLog.begin() + 10000);
    }
}

uint32_t MacroManager::frameForPosition(float x) const {
    uint32_t best = 0;
    float bestX = -1e9f;
    for (auto& sample : m_positionLog) {
        if (sample.first <= x && sample.first > bestX) {
            bestX = sample.first;
            best = sample.second;
        }
    }
    return best;
}

void MacroManager::truncateToFrame(uint32_t frame) {
    auto& events = m_buffer.events;
    events.erase(
        std::remove_if(events.begin(), events.end(),
            [frame](const InputEvent& ev) { return ev.frame > frame; }),
        events.end()
    );
    m_positionLog.erase(
        std::remove_if(m_positionLog.begin(), m_positionLog.end(),
            [frame](const std::pair<float, uint32_t>& sample) { return sample.second > frame; }),
        m_positionLog.end()
    );
}

void MacroManager::onLevelReset(float respawnX, bool practiceMode) {
    if (m_mode == Mode::Recording) {
        if (!m_haveStartX) {
            // No real gameplay has happened yet — this is almost
            // certainly the reset that fires when practice mode itself
            // first kicks in, immediately after pressing Record (before
            // a single position sample has been logged). There's
            // nothing to end or roll back yet: just absorb it and keep
            // Recording. The very next logPosition() call, once actual
            // gameplay resumes, establishes the session's real starting
            // position. Without this check, that initial reset was being
            // misread as "genuine restart" every time (m_haveStartX &&
            // ... short-circuits to false when it's false), ending the
            // session before the player had even started playing.
            m_frame = 0;
            return;
        }

        // A reset at the level start is ambiguous: it can be a genuine
        // restart, but it can also be a normal practice-mode death before
        // the first checkpoint. Practice mode is therefore the authority
        // for whether this reset ends the recording session.
        if (practiceMode) {
            // A checkpoint respawn lands meaningfully past the session's
            // recorded starting X. Roll back to the most recent recorded
            // frame at that position, discarding only the failed attempt.
            // A respawn at the start is a normal practice-mode death (or
            // restart), so roll back to frame 0 and keep Recording.
            bool isCheckpointRespawn = std::fabs(respawnX - m_sessionStartX) >= 8.f;

            if (isCheckpointRespawn) {
                uint32_t resumeFrame = frameForPosition(respawnX);
                truncateToFrame(resumeFrame);
                m_frame = resumeFrame;
                log::info("MacroBot: checkpoint respawn — resuming recording at frame {}", m_frame);
            } else {
                truncateToFrame(0);
                m_frame = 0;
                log::info("MacroBot: practice-mode reset at start — restarting recording from frame 0");
            }
            return;
        }

        // Practice mode has ended. Now, and only now, finish the current
        // recording and wait for Save. Trim trailing artifacts first: an
        // unreleased press (GD's hold-jump-to-restart gesture, or a jump
        // simply cut short by death), then anything landing on essentially
        // the same tick as this reset (GD force-releasing a still-held
        // button as part of its own death cleanup).
        trimTrailingUnreleasedPresses(m_buffer.events);
        trimTrailingEventsAtFrame(m_buffer.events, m_frame);
        m_buffer.totalFrames = m_frame;
        m_mode = Mode::Standby;
        m_positionLog.clear();
        m_haveStartX = false;
        log::info("MacroBot: practice session finished ({} events, {} frames) — waiting for Save",
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
    m_positionLog.clear();
    m_haveStartX = false;
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
