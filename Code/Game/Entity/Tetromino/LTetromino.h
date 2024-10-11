#pragma once
#include "BaseTetromino.hpp"

class LTetromino: public BaseTetromino
{
public:
    LTetromino(const IntVec2& startPos);
    LTetromino(const IntVec2& startPos, Grid* parentGrid);
};
