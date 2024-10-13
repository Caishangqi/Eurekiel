#include "BaseTetromino.hpp"
#include "Game/Grid/Grid.hpp"

BaseTetromino::~BaseTetromino()
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (m_cubes[i][j] != nullptr)
            {
                m_cubes[i][j]->m_isGarbage = true;
                m_cubes[i][j]              = nullptr;
            }
        }
    }

    delete[] m_cubes;

    m_parentGrid = nullptr;
}

BaseTetromino::BaseTetromino(IntVec2 startPos, Grid* parentGrid)
{
    m_posGrid    = startPos;
    m_parentGrid = parentGrid;
}

BaseTetromino::BaseTetromino(IntVec2 startPos)
{
    m_posGrid = startPos;
}

BaseTetromino* BaseTetromino::InitTetromino()
{
    // 39 19
    // 3  1
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (m_layout[i][j] == true)
            {
                // 
                auto  spawnPos = IntVec2(m_posGrid.x + j, m_posGrid.y - i);
                Cube* cube     = m_parentGrid->PlaceCube(spawnPos);
                cube->SetParentTetromino(this);
                m_cubes[i][j]                = cube;
                cube->m_EnablePhysicalRadius = true; // Enable Physical Radius that use for structure
                cube->m_TetrominoPosition    = IntVec2(i, j); // save the relative position of cube in tetro
            }
        }
    }

    return this;
}

bool BaseTetromino::RemoveCubePointerInTetromino(Cube* cube)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (m_cubes[i][j] != nullptr && m_cubes[i][j] == cube)
            {
                m_cubes[i][j] = nullptr;
                return true;
            }
        }
    }
    return false;
}

bool BaseTetromino::MarkCubeAsGarbage(Cube* cube)
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (m_cubes[i][j] != nullptr && m_cubes[i][j] == cube)
            {
                m_cubes[i][j]->m_isGarbage = true;
                return true;
            }
        }
    }
    return false;
}

void BaseTetromino::SetParentGrid(Grid* parentGrid)
{
    m_parentGrid = parentGrid;
}

Cube* BaseTetromino::GetChildCubes()
{
    Cube* cube[8]   = {nullptr};
    int   sizeIndex = 0;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (m_cubes[i][j] != nullptr)
            {
                cube[sizeIndex] = m_cubes[i][j];
                sizeIndex++;
            }
        }
    }
    return *cube;
}

void BaseTetromino::SetChildCubesStatic()
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (m_cubes[i][j] != nullptr)
            {
                Cube* cube = m_cubes[i][j];
                cube->SetCubeStatic();
                cube->UpdateGridPosition();
                cube->CorrectWorldPosition();

                m_parentGrid->m_cubesData[cube->m_gridPos.y][cube->m_gridPos.x] = cube;
            }
        }
    }
}
