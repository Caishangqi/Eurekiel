#pragma once
#include "BaseTetromino.hpp"

class JTetromino : public BaseTetromino
{
public:
    JTetromino(const IntVec2& startPos);
    JTetromino(const IntVec2& startPos, Grid* parentGrid);
};
