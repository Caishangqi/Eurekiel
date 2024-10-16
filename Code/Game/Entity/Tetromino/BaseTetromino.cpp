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

bool BaseTetromino::IsAllCubeDie()
{
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (m_cubes[i][j] != nullptr)
            {
                Cube* cube = m_cubes[i][j];
                if (cube->IsAlive())
                {
                    return false;
                }
            }
        }
    }
    return true;
}

int BaseTetromino::GetNumCubesDefined()
{
    int count = 0;
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            if (m_layout[i][j])
            {
                count++;
            }
        }
    }
    return count;
}

bool BaseTetromino::SetDeltaPosition(IntVec2 deltaPos)
{
    if (m_posGrid != IntVec2(-1, -1))
    {
        bool IsEveryCubeCheckMoveValidation = true;
        // 39 19  Original
        // Move Down
        // 39 18  New

        // 0  -1   Displacement (Delta)
        IntVec2 newPos = m_posGrid + deltaPos;


        // Checking
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                if (m_cubes[i][j] != nullptr && !m_cubes[i][j]->m_IsStatic) // Static cube can not move
                {
                    Cube* cube = m_cubes[i][j];
                    // Delta Displacement grid
                    IntVec2 cubeNewPosition = cube->m_gridPos + deltaPos;
                    if (cubeNewPosition.y > 19 || cubeNewPosition.y < 0)
                    {
                        IsEveryCubeCheckMoveValidation = false;
                    }
                }
            }
        }

        if (IsEveryCubeCheckMoveValidation == false)
        {
            return false;
        }

        // Moving
        for (int i = 0; i < 4; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                if (m_cubes[i][j] != nullptr && !m_cubes[i][j]->m_IsStatic) // Static cube can not move
                {
                    Cube* cube       = m_cubes[i][j];
                    cube->m_gridPos  = cube->m_gridPos + deltaPos;
                    cube->m_position = cube->m_position + Grid::ConvertGridPositionToWorldPosition(deltaPos);
                }
            }
        }

        m_posGrid = newPos;
    }
    return false;
}
