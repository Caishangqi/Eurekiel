#pragma once

#pragma once
#include <vector>

// Hierarchical clock that inherits time scale.
// Parent clocks pass scaled delta seconds down to child clocks as their base delta seconds.
// Child clocks in turn scale that time and pass it down to their children.
// There is one system clock at the root of the hierarchy.
class Clock
{
public:
    // Default constructor, uses the system clock as the parent of the new clock.
    Clock();

    // Constructor to specify a parent clock for the new clock.
    explicit Clock(Clock& parent);

    // Destructor, Un-parents itself and its children to avoid crashes but does not otherwise fix the hierarchy.
    ~Clock();
    Clock(const Clock& copy) = delete;

    // Reset all bookkeeping variables back to zero and set the last updated time to the current system time.
    void Reset();

    bool IsPaused() const;
    void Pause();
    void Unpause();
    void TogglePause();

    // Unpause for one frame, then pause again.
    void StepSingleFrame();

    // Set and get the time scale value for this clock.
    void  SetTimeScale(float timeScale);
    float GetTimeScale() const;

    float GetDeltaSeconds() const;
    float GetTotalSeconds() const;
    float GetFrameRate() const;
    int   GetFrameCount() const;

    // Get a reference to the static system clock that acts as the default parent clock.
    static Clock& GetSystemClock();

    // Tick the system clock, updating the entire hierarchy.
    static void TickSystemClock();

protected:
    // Tick function to calculate delta seconds and update time.
    void Tick();

    // Advance the clock, updating bookkeeping variables and propagating changes to child clocks.
    void Advance(float deltaTimeSeconds);

    // Manage child clocks.
    void AddChild(Clock* childClock);
    void RemoveChild(Clock* childClock);

    Clock*              m_parent = nullptr; // Parent clock, nullptr for root clock.
    std::vector<Clock*> m_children; // Child clocks.

    // Bookkeeping variables.
    float m_lastUpdateTimeInSeconds = 0.0;
    float m_totalSeconds            = 0.0;
    float m_deltaSeconds            = 0.0;
    int   m_frameCount              = 0;

    float m_timeScale       = 1.0; // Time scale factor.
    bool  m_isPaused        = false; // Pause state.
    bool  m_stepSingleFrame = false; // Single frame step mode.
    float m_maxDeltaSeconds = 0.1f; // Maximum delta time for debugging.

    static Clock* g_theSystemClock;
};
