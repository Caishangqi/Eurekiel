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

Cube* Grid::PlaceCube(IntVec2 gridPos)
{
    Cube* entityCube;
    entityCube             = new Cube(m_game, this, Vec2(), 0.0f);
    entityCube->m_gridPos  = gridPos;
    entityCube->m_position = GetPositionFromGrid(gridPos);


    for (Cube*& element : m_game->m_cube)
    {
        if (element == nullptr)
        {
            element = entityCube;
            printf("[entity]    Spawn 1 Entity - type: Cube\n");
            break;
        }
    }
    return entityCube;
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
