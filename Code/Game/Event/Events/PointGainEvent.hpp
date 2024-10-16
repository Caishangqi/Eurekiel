#pragma once
#include "Game/Event/Event.hpp"

struct PointGainEvent : public Event
{
public:
    int m_gainedScore = 0;

    PointGainEvent(int _gainedScore) : m_gainedScore(_gainedScore)
    {
    }
};
