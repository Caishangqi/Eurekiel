#include "Grid.hpp"

#include "Engine/Math/IntVec2.hpp"
#include "Game/Game.hpp"
#include "Game/Resource/SoundRes.hpp"

Grid::Grid(Game* game)
{
    m_game = game;
    printf(
        "[grid]     Initialize Grid System...\n"
    );
}

Grid::~Grid()
{
    m_game = nullptr;
}

void Grid::Update(float deltaSeconds)
{
}

Cube* Grid::PlaceCube(IntVec2 gridPos)
{
    Cube* entityCube;
    entityCube             = new Cube(m_game, this, Vec2(), 0.0f);
    entityCube->m_gridPos  = gridPos;
    entityCube->m_position = GetPositionFromGrid(gridPos);

    for (int i = 0; i < 1024; i++)
    {
        if (m_game->m_cube[i] == nullptr)
        {
            m_game->m_cube[i]        = entityCube;
            entityCube->m_internalID = i;
            printf("[entity]    Spawn 1 Entity - type: Cube\n");
            break;
        }
    }

    return entityCube;
}

BaseTetromino* Grid::PlaceTetromino(BaseTetromino* baseTetromino)
{
    return baseTetromino->InitTetromino();
}


// 19
float Grid::GetHorizontalBaseLine(int horizontalIndex)
{
    for (int x = GRID_WIDTH_SIZE - 1; x >= 0; x--)
    {
        if (m_cubesData[horizontalIndex][x] != nullptr)
        {
            return m_cubesData[horizontalIndex][x]->m_position.x + 5.0f;
        }
    }
    return GRID_BASE_LINE_X;
}


void Grid::OnCubeTouchBaseLineEvent(Cube* cube)
{
    printf(
        "[event]     OnCubeTouchBaseLineEvent Trigger\n"
    );
    IntVec2 gridPos = GetGridPositionFromPosition(cube->m_position);

    // Update cubes data grid array
    m_cubesData[gridPos.y][gridPos.x] = cube;

    //Correct position
    cube->m_position = GetPositionFromGrid(gridPos);
    cube->m_aabb->SetCenter(cube->m_position);

    // if cube have parent tet the set all cube in tet static
    BaseTetromino* tet = cube->GetParentTetromino();
    if (tet)
    {
        tet->SetChildCubesStatic();
    }

    for (int i = 0; i < GRID_WIDTH_SIZE; i++)
    {
        if (CheckVerticalFull(i))
        {
            MarkVerticalFullGarbage(i);
            RemoveVerticalFullPointer(i);
        }
    }
}

// (39, 19)
Vec2 Grid::GetPositionFromGrid(const IntVec2& gridPos)
{
    float x = 2.5f + static_cast<float>(gridPos.x) * 5.0f;
    float y = 2.5f + static_cast<float>(gridPos.y) * 5.0f;
    return Vec2(x, y);
}

IntVec2 Grid::GetGridPositionFromPosition(const Vec2& position)
{
    return IntVec2(static_cast<int>((position.x) / 5.f), static_cast<int>(position.y / 5.f));
}

bool Grid::RemoveCubePointerInGrid(Cube* cube)
{
    for (int i = 0; i < GRID_HEIGHT_SIZE; i++)
    {
        for (int j = 0; j < GRID_WIDTH_SIZE; j++)
        {
            if (m_cubesData[i][j] != nullptr && m_cubesData[i][j] == cube)
            {
                m_cubesData[i][j] = nullptr;
                return true;
            }
        }
    }
    return false;
}

bool Grid::CheckVerticalFull(int verticalIndex)
{
    for (int i = 0; i < GRID_HEIGHT_SIZE; i++)
    {
        if (m_cubesData[i][verticalIndex] == nullptr)
        {
            return false;
        }
    }
    m_NeedCancelCubes = true;
    return true;
}

bool Grid::RemoveVerticalFullPointer(int verticalIndex)
{
    for (int i = 0; i < GRID_HEIGHT_SIZE; i++)
    {
        m_cubesData[i][verticalIndex] = nullptr;
    }
    return true;
}

bool Grid::MarkVerticalFullGarbage(int verticalIndex)
{
    for (int i = 0; i < GRID_HEIGHT_SIZE; i++)
    {
        if (m_cubesData[i][verticalIndex])
        {
            m_cubesData[i][verticalIndex]->m_isGarbage = true;
            BaseTetromino* tet                         = m_cubesData[i][verticalIndex]->GetParentTetromino();
            if (tet)
            {
                tet->MarkCubeAsGarbage(m_cubesData[i][verticalIndex]);
                tet->RemoveCubePointerInTetromino(m_cubesData[i][verticalIndex]);
            }
        }
    }
    return true;
}

Cube* Grid::GetVerticalFullCubes(int verticalIndex)
{
    Cube* cubeList[GRID_HEIGHT_SIZE] = {nullptr};
    for (int i = GRID_HEIGHT_SIZE - 1; i < 0; i--)
    {
        if (m_cubesData[i][verticalIndex])
        {
            cubeList[i] = m_cubesData[i][verticalIndex];
        }
    }
    return *cubeList;
}
