#pragma once
#include "Game/Event/Event.hpp"
class Game;

struct PointGainEvent : public TypedEvent<PointGainEvent>
{
public:
    int   m_gainedScore = 0;
    Game* m_game        = nullptr;

    PointGainEvent(int gainedScore, Game* game)
    {
        m_gainedScore = gainedScore;
        m_game        = game;
    }

    PointGainEvent(int gainedScore)
    {
        m_gainedScore = gainedScore;
    }

    ~PointGainEvent()
    {
        m_game = nullptr;
    }
};
