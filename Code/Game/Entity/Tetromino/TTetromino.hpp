#pragma once
#include "BaseTetromino.hpp"

class TTetromino : public BaseTetromino
{
public:
    TTetromino(const IntVec2& startPos);
    TTetromino(const IntVec2& startPos, Grid* parentGrid);
};
