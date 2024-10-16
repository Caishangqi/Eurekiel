#pragma once
#include "Game/Entity/Tetromino/BaseTetromino.hpp"
#include "Game/Event/Event.hpp"

struct TetrominoAllChildDieEvent : public Event
{
public:
    BaseTetromino * targetTetromino = nullptr;

    TetrominoAllChildDieEvent(BaseTetromino * targetTetromino) : targetTetromino(targetTetromino)
    {
        
    }
};
