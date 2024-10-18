#include "PointGainListener.hpp"
#include <cstdio>
#include "Game/Game.hpp"

void PointGainListener::onEvent(PointGainEvent* event)
{
    event->m_game->Score += event->m_gainedScore;
}
