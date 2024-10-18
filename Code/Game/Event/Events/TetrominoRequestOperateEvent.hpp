#pragma once
#include "Game/Enum/EDirection.h"
#include "Game/Event/Event.hpp"

struct TetrominoRequestOperateEvent : public TypedEvent<TetrominoRequestOperateEvent>
{
public:
    EDirection requestedOperatedDirection = EDirection::UP;

    TetrominoRequestOperateEvent(EDirection requestedOperatedDirection) : requestedOperatedDirection(
        requestedOperatedDirection)
    {
    }
};
