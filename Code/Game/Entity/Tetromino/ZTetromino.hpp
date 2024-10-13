#pragma once
#include "BaseTetromino.hpp"

class ZTetromino : public BaseTetromino
{
public:
    ZTetromino(const IntVec2& startPos);
    ZTetromino(const IntVec2& startPos, Grid* parentGrid);
};
