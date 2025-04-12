#include "Timer.hpp"

#include "Clock.hpp"
#include "StringUtils.hpp"

Timer::Timer()
{
}

Timer::Timer(float period, const Clock* clock): m_clock(clock), m_period(period)
{
    if (m_clock == nullptr)
        m_clock = &Clock::GetSystemClock();
}

void Timer::Start()
{
    m_startTime = m_clock->GetTotalSeconds();
}

void Timer::Stop()
{
    m_startTime = 0.f;
}

float Timer::GetElapsedTime() const
{
    if (m_startTime < 0.0)
        return 0.0;
    return m_clock->GetTotalSeconds() - m_startTime;
}

float Timer::GetElapsedFraction() const
{
    if (m_period == 0.0)
        return 0.0;
    return GetElapsedTime() / m_period;
}

bool Timer::IsStopped() const
{
    return m_startTime <= 0.0;
}

bool Timer::HasPeriodElapsed() const
{
    if (m_startTime < 0.0)
        return false;
    return GetElapsedTime() >= m_period;
}

bool Timer::DecrementPeriodIfElapsed()
{
    if (HasPeriodElapsed())
    {
        //printf("Timer Period End %f\n", GetElapsedTime());
        m_startTime += m_period;
        return true;
    }
    return false;
}
