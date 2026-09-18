#pragma once
#include "MacroFormat.hpp"
#include <Geode/Geode.hpp>
#include <filesystem>
#include <optional>

namespace macrobot {

enum class Mode {
    Idle,
    Recording,
    Playing
};

// Owns the single source of truth for "are we recording / playing right
// now", the in-progress buffer, and disk IO. One instance for the whole
// mod (GD only ever has one active PlayLayer anyway).
class MacroManager {
public:
    static MacroManager& get();

    // --- Transport controls, called from the bot menu UI ---------------
    void startRecording();
    // Called when the practice session ends (leaving PlayLayer while
    // recording, or the level is completed). Writes the buffer to disk
    // named after the level and clears state.
    void finishRecordingAndSave();
    void cancelRecording();

    // Loads `path` into the "armed" macro slot. Does not start playback.
    bool loadMacroFromFile(const std::filesystem::path& path);

    // Starts playing whatever is currently armed (from loadMacroFromFile,
    // or the macro that was just recorded). No-op if nothing is armed.
    void startPlaying();
    void stopPlaying();

    Mode mode() const { return m_mode; }
    bool hasArmedMacro() const { return m_armed.has_value(); }
    std::optional<std::string> armedMacroName() const;

    // --- Hooks into these from PlayLayer, see main.cpp ------------------
    // Called once per physics tick (i.e. once per PlayLayer::update call).
    // Advances the step counter and, if playing back, fires any due events
    // via the callback.
    void onPhysicsStep(const std::function<void(uint8_t button, bool player1, bool down)>& fireInput);

    // Called from the handleButton hook. If we're recording, stores the
    // transition against the current step. No-op otherwise.
    void recordInput(int button, bool player1, bool down);

    // Called when the level is (re)started, e.g. PlayLayer::resetLevel.
    // While recording this clears the buffer so only the final/latest
    // attempt survives (matches how practice-mode macros are expected to
    // behave: each retry starts the recording over).
    void onLevelReset();

    void setLevelName(const std::string& name) { m_levelName = name; }

    std::filesystem::path macrosDir() const;

private:
    MacroManager() = default;

    Mode m_mode = Mode::Idle;
    std::string m_levelName;
    uint64_t m_step = 0;
    size_t m_playCursor = 0;

    MacroData m_buffer;                    // currently being recorded
    std::optional<MacroData> m_armed;      // loaded / just-recorded, ready to play
    std::optional<std::string> m_armedDisplayName;
};

}
