#pragma once
#include "BaseTetromino.hpp"

class ITetromino : public BaseTetromino
{
public:
    ITetromino(const IntVec2& startPos);
    ITetromino(const IntVec2& startPos, Grid* parentGrid);
};
