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

    // --- Debug HUD (toggleable frame + event counters) -----------------
    bool isDebugHudEnabled() const { return m_debugHudEnabled; }
    void setDebugHudEnabled(bool enabled) { m_debugHudEnabled = enabled; }

    uint32_t currentFrame() const { return m_frame; }
    size_t recordedEventCount() const { return m_buffer.events.size(); }
    size_t playedEventCount() const { return m_playCursor; }
    size_t totalArmedEventCount() const { return m_armed ? m_armed->events.size() : 0; }

    // Hands the manager the current level's two HUD labels (created in
    // PlayLayer::init, see main.cpp) so updateHud() can drive them. A
    // method added to a $modify(PlayLayer) class isn't actually part of
    // PlayLayer's real interface as seen from other classes/files — that
    // was the "no member named updateHud in PlayLayer" build error — so
    // the labels are routed through this always-accessible singleton
    // instead, and updateHud() (called from the PlayerObject::update
    // hook, which is proven to actually fire) does the refreshing here.
    void setHudLabels(cocos2d::CCLabelBMFont* frameLabel, cocos2d::CCLabelBMFont* eventsLabel);
    void updateHud();

    // --- Hooks into these from PlayerObject::update, see main.cpp -------
    // Called once per actual physics substep (PlayerObject::update fires
    // once per real GD physics tick, not once per rendered frame the way
    // PlayLayer::update does — GD runs several physics substeps inside a
    // single rendered frame). This is the correct, verified granularity
    // for frame-accurate macro playback. Called BEFORE
    // PlayerObject::update() itself runs so a due input takes effect in
    // the same substep a live touch would have (touches/keys dispatch
    // before the scheduler's update calls in cocos2d's frame loop).
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
    uint32_t m_frame = 0;
    size_t m_playCursor = 0;
    bool m_debugHudEnabled = false;

    MacroData m_buffer;                    // recording / standby buffer
    std::optional<MacroData> m_armed;      // loaded / just-saved, ready to play
    std::optional<std::string> m_armedDisplayName;

    cocos2d::CCLabelBMFont* m_hudFrameLabel = nullptr;
    cocos2d::CCLabelBMFont* m_hudEventsLabel = nullptr;
};

}
