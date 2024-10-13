#include "STetromino.hpp"

STetromino::STetromino(const IntVec2& startPos): BaseTetromino(startPos)
{
    m_layout[0][0] = true;
    m_layout[1][0] = true;
    m_layout[1][1] = true;
    m_layout[2][0] = true;
}

STetromino::STetromino(const IntVec2& startPos, Grid* parentGrid): BaseTetromino(startPos, parentGrid)
{
    m_layout[0][0] = true;
    m_layout[1][0] = true;
    m_layout[1][1] = true;
    m_layout[2][0] = true;
}
