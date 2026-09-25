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

    // Ends the active recording without discarding the captured attempt.
    // The result enters Standby so it can be saved with the Save button.
    void stopRecording();

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

    // Refreshes (and lazily creates, first call) the debug HUD's two
    // labels. Deliberately NOT dependent on PlayLayer::init() handing
    // anything off — the labels are created here, the first time this is
    // called with a running scene available, and parented directly to
    // the scene root rather than to PlayLayer. That removes any
    // dependency on PlayLayer-specific init ordering or coordinate space
    // entirely; the only remaining requirement is that SOMETHING calls
    // updateHud() periodically, which PlayerObject::update already does
    // (proven firing, since it's what drives playback).
    void updateHud();

    // Destroys the HUD labels (if created) and clears them. Call this on
    // leaving a level (PlayLayer::onQuit) so they don't linger attached
    // to a scene that's about to go away.
    void clearHud();

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

    // Called from PlayerObject::update (see main.cpp) while Recording,
    // with the player's current X position. Used to build a lightweight
    // log matching "position reached" to "frame it happened on", so a
    // later checkpoint respawn (see onLevelReset) can figure out which
    // frame to roll back to. No-op outside Recording.
    void logPosition(float x);

    // Called when PlayLayer::resetLevel() fires, after the player's
    // position has settled for the respawn. Practice mode is passed in
    // explicitly because a reset at the level start is NOT enough to
    // determine whether the recording session has ended: dying before
    // the first checkpoint also respawns at the beginning.
    //
    // While practice mode is active, every reset belongs to the same
    // recording session: checkpoint deaths roll back to the checkpoint
    // frame, while deaths/restarts at the beginning roll back to frame 0.
    // The recording only moves to Standby once practice mode is no longer
    // active.
    void onLevelReset(float respawnX, bool practiceMode);

    void setLevelName(const std::string& name) { m_levelName = name; }

    std::filesystem::path macrosDir() const;

private:
    MacroManager() = default;

    void ensureHudLabels();
    uint32_t frameForPosition(float x) const;
    void truncateToFrame(uint32_t frame);

    Mode m_mode = Mode::Idle;
    std::string m_levelName;
    uint32_t m_frame = 0;
    size_t m_playCursor = 0;
    bool m_debugHudEnabled = false;

    MacroData m_buffer;                    // recording / standby buffer
    std::optional<MacroData> m_armed;      // loaded / just-saved, ready to play
    std::optional<std::string> m_armedDisplayName;

    // Position -> frame log, built while Recording, used to figure out
    // which frame a checkpoint respawn should roll back to.
    std::vector<std::pair<float, uint32_t>> m_positionLog;
    float m_sessionStartX = 0.f;
    bool m_haveStartX = false;
    int m_logThrottle = 0;

    cocos2d::CCLabelBMFont* m_hudFrameLabel = nullptr;
    cocos2d::CCLabelBMFont* m_hudEventsLabel = nullptr;
};

}
