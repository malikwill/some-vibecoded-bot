#pragma once
#include "MacroFormat.hpp"
#include <Geode/Geode.hpp>
#include <filesystem>
#include <optional>

namespace macrobot {

enum class Mode {
    Idle,
    Recording,  // actively capturing input for the current attempt
    Standby,    // one attempt was captured and the session ended; waiting
                // for the user to hit Save. New attempts are NOT recorded
                // while in this state.
    Playing
};

// Owns the single source of truth for "are we recording / on standby /
// playing right now", the in-progress buffer, and disk IO. One instance
// for the whole mod (GD only ever has one active PlayLayer anyway).
class MacroManager {
public:
    static MacroManager& get();

    // --- Transport controls, called from the bot menu UI ---------------

    // Begins capturing input for a fresh attempt. Discards anything
    // previously pending (recording or standby).
    void startRecording();

    // Writes whatever's currently pending (must be in Standby, i.e. a
    // finished attempt that hasn't been saved yet) to disk, named after
    // the level, and arms it so Play works immediately. No-op if there's
    // nothing pending.
    void saveStandbyMacro();

    // Discards whatever's pending (Recording or Standby) without saving.
    void cancelRecording();

    // Loads `path` into the "armed" macro slot. Does not start playback.
    bool loadMacroFromFile(const std::filesystem::path& path);

    // Starts playing whatever is currently armed (from loadMacroFromFile,
    // or a macro that was just saved). No-op if nothing is armed.
    void startPlaying();
    void stopPlaying();

    Mode mode() const { return m_mode; }
    bool hasArmedMacro() const { return m_armed.has_value(); }
    bool hasPendingSave() const { return m_mode == Mode::Standby; }
    std::optional<std::string> armedMacroName() const;

    // --- Hooks into these from PlayLayer, see main.cpp ------------------
    // Called once per physics tick (i.e. once per PlayLayer::update call).
    // Advances the step counter and, if playing back, fires any due events
    // via the callback.
    void onPhysicsStep(const std::function<void(uint8_t button, bool player1, bool down)>& fireInput);

    // Called from the handleButton hook. Only stores the transition while
    // actively Recording; no-op in every other mode (in particular, a
    // no-op while on Standby — that's the whole point of Standby).
    void recordInput(int button, bool player1, bool down);

    // Called when the level is (re)started, e.g. PlayLayer::resetLevel.
    // If we were Recording, this is "the session finished": stop
    // capturing (move to Standby) but keep what was captured. Attempts
    // made after this point are not recorded until the user Saves (which
    // returns to Idle) or hits Record again (which discards and restarts).
    void onLevelReset();

    void setLevelName(const std::string& name) { m_levelName = name; }

    std::filesystem::path macrosDir() const;

private:
    MacroManager() = default;

    Mode m_mode = Mode::Idle;
    std::string m_levelName;
    uint64_t m_step = 0;
    size_t m_playCursor = 0;

    MacroData m_buffer;                    // recording / standby buffer
    std::optional<MacroData> m_armed;      // loaded / just-saved, ready to play
    std::optional<std::string> m_armedDisplayName;
};

}
