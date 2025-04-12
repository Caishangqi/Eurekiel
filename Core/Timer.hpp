#pragma once

class Clock;

// Timer class that can attach to any clock in a hierarchy and handle duration tracking.
class Timer
{
public:
    // Create a timer with a period and a specified clock. If null, use the system clock.
    Timer();
    explicit Timer(float period, const Clock* clock = nullptr);

    // Control the timer.
    void Start();
    void Stop();

    // Get elapsed time since the timer started.
    float GetElapsedTime() const;
    float GetElapsedFraction() const;

    // Timer status checks.
    bool IsStopped() const;
    bool HasPeriodElapsed() const;

    // If a period has elapsed, decrement it.
    bool DecrementPeriodIfElapsed();

protected:
    const Clock* m_clock     = nullptr; // Clock reference.
    float        m_startTime = 0.0; // Timer start time.
    float        m_period    = 0.0; // Time interval for period.
};
