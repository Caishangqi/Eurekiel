#include "Clock.hpp"

#include "Time.hpp"
Clock* Clock::g_theSystemClock = new Clock();

Clock::Clock()
{
}

Clock::Clock(Clock& parent) : m_parent(&parent)
{
    if (m_parent)
        m_parent->AddChild(this);
}

Clock::~Clock()
{
    for (Clock* m_child : m_children)
    {
        delete m_child;
        m_child = nullptr;
    }
}

void Clock::Reset()
{
}

bool Clock::IsPaused() const
{
    return m_isPaused;
}

void Clock::Pause()
{
    m_isPaused = true;
}

void Clock::Unpause()
{
    m_isPaused = false;
}

void Clock::TogglePause()
{
}

void Clock::StepSingleFrame()
{
    m_stepSingleFrame = true;
}

void Clock::SetTimeScale(float timeScale)
{
    m_timeScale = timeScale;
}

float Clock::GetTimeScale() const
{
    return m_timeScale;
}

float Clock::GetDeltaSeconds() const
{
    return m_deltaSeconds;
}

float Clock::GetTotalSeconds() const
{
    return m_totalSeconds;
}

float Clock::GetFrameRate() const
{
    if (m_deltaSeconds > 0.0)
        return 1.0f / m_deltaSeconds;
    return 0.0;
}

int Clock::GetFrameCount() const
{
    return m_frameCount;
}

Clock& Clock::GetSystemClock()
{
    return *g_theSystemClock;
}

void Clock::TickSystemClock()
{
    GetSystemClock().Tick();
}

void Clock::Tick()
{
    float timeNow = static_cast<float>(GetCurrentTimeSeconds());
    Advance(timeNow - m_lastUpdateTimeInSeconds);
}

void Clock::Advance(float deltaTimeSeconds)
{
    if (m_isPaused && !m_stepSingleFrame)
    {
        m_deltaSeconds            = 0.0f;
        m_lastUpdateTimeInSeconds = static_cast<float>(GetCurrentTimeSeconds());
        return;
    }

    if (m_stepSingleFrame)
    {
        m_stepSingleFrame = false;
    }

    float scaledDelta = deltaTimeSeconds * m_timeScale;
    m_frameCount++;
    m_deltaSeconds = scaledDelta;
    m_totalSeconds += scaledDelta;
    m_lastUpdateTimeInSeconds += deltaTimeSeconds;
    for (Clock* child : m_children)
    {
        child->Advance(scaledDelta);
    }
}

void Clock::AddChild(Clock* childClock)
{
    m_children.push_back(childClock);
}

void Clock::RemoveChild(Clock* childClock)
{
    auto it = std::find(m_children.begin(), m_children.end(), childClock);
    if (it != m_children.end())
    {
        m_children.erase(it);
    }
}
