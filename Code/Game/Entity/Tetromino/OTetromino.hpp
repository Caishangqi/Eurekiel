#pragma once
#include "BaseTetromino.hpp"

class OTetromino : public BaseTetromino
{
public:
    OTetromino(const IntVec2& startPos);
    OTetromino(const IntVec2& startPos, Grid* parentGrid);
};
