#pragma once
#include "BaseTetromino.hpp"

class STetromino : public BaseTetromino
{
public:
    STetromino(const IntVec2& startPos);
    STetromino(const IntVec2& startPos, Grid* parentGrid);
};
