#pragma once
#include "Game/Enum/EGameState.h"
#include "Game/Event/Event.hpp"

struct GameChangeStateEvent : public Event
{
public:
    EGameState targetGameState;
    EGameState preGameState;

    GameChangeStateEvent(EGameState preGameState, EGameState targetGameState) : targetGameState(targetGameState),
        preGameState(preGameState)
    {
    }
};
