#include "Grid.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Game/Game.hpp"
#include "Game/Entity/Tetromino/BaseTetromino.hpp"

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
    // TODO Try use event system but probably need Recursion
    MoveCubesRightOfEmptyColumnLeft(GetMostNearEmptyVerticalIndex());
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
            OnCubeCancelEvent(i);
        }
    }

    IsCubeOutOfBounds();
}

void Grid::OnCubeCancelEvent(int verticalIndex)
{
    printf(
        "[event]     OnCubeCancelEvent Trigger\n"
        "                   > Target vertical Index: %d\n", verticalIndex
    );
}

void Grid::OnCubeTouchBoundEvent()
{
    printf(
        "[event]     OnCubeTouchBoundEvent Trigger\n"
    );
}

void Grid::OnCubeDieEvent(Cube* cube)
{
    printf(
        "[event]     OnCubeDieEvent Trigger\n"
    );
    if (cube->GetParentTetromino())
    {
        if (cube->GetParentTetromino()->IsAllCubeDie())
        {
            OnTetrominoDieEvent(cube->GetParentTetromino());
        }
    }
}

void Grid::OnTetrominoDieEvent(BaseTetromino* tetromino)
{
    printf(
        "[event]     OnTetrominoDieEvent Trigger\n"
    );

    g_theAudio->StartSound(SOUND::DESTROY_TETROMINO);

    // Super score 2 ^ num of cube
    int rewardsWholeTetromino = static_cast<int>(pow(2, tetromino->GetNumCubesDefined()));

    m_game->OnPointGainEvent(rewardsWholeTetromino);

    for (BaseTetromino* m_tetromino_data : m_TetrominoData)
    {
        if (m_tetromino_data == tetromino)
        {
            delete m_tetromino_data;
            m_tetromino_data = nullptr;
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

bool Grid::CheckVerticalHasCube(int verticalIndex)
{
    for (int row = 0; row < GRID_HEIGHT_SIZE; ++row)
    {
        if (m_cubesData[row][verticalIndex] != nullptr)
        {
            return true;
        }
    }
    return false;
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
    for (int i = 0; i < GRID_HEIGHT_SIZE; i++)
    {
        if (m_cubesData[i][verticalIndex])
        {
            cubeList[i] = m_cubesData[i][verticalIndex];
        }
    }
    return *cubeList;
}

// 2                        // 3
void Grid::MoveVerticalCubes(int targetVerticalIndex, int fromVerticalIndex)
{
    for (int i = 0; i < GRID_HEIGHT_SIZE; i++)
    {
        m_cubesData[i][targetVerticalIndex] = m_cubesData[i][fromVerticalIndex];
        RemoveVerticalFullPointer(fromVerticalIndex);
    }
    // 
}

int Grid::GetMostNearEmptyVerticalIndex()
{
    for (int verticalIndex = 0; verticalIndex < GRID_WIDTH_SIZE; ++verticalIndex)
    {
        bool isEmpty = true;
        // 检查这一列是否完全为空
        for (int row = 0; row < GRID_HEIGHT_SIZE; ++row)
        {
            if (m_cubesData[row][verticalIndex] != nullptr)
            {
                isEmpty = false;
                break;
            }
        }
        // 如果找到一个空的竖直列，返回其索引
        if (isEmpty)
        {
            return verticalIndex;
        }
    }

    return -1;
}

void Grid::MoveCubesRightOfEmptyColumnLeft(int emptyColumnIndex)
{
    if (emptyColumnIndex == -1)
    {
        return;
    }

    for (int verticalIndex = emptyColumnIndex + 1; verticalIndex < GRID_WIDTH_SIZE; ++verticalIndex)
    {
        for (int row = 0; row < GRID_HEIGHT_SIZE; ++row)
        {
            if (m_cubesData[row][verticalIndex] != nullptr)
            {
                // -1 for safe check
                // move the cube towards left empty space
                m_cubesData[row][verticalIndex - 1] = m_cubesData[row][verticalIndex];
                m_cubesData[row][verticalIndex]     = nullptr;

                // update position and collision
                m_cubesData[row][verticalIndex - 1]->m_gridPos  = IntVec2(verticalIndex - 1, row);
                m_cubesData[row][verticalIndex - 1]->m_position = GetPositionFromGrid(
                    m_cubesData[row][verticalIndex - 1]->m_gridPos);
                m_cubesData[row][verticalIndex - 1]->m_aabb->SetCenter(m_cubesData[row][verticalIndex - 1]->m_position);
            }
        }
    }
}

bool Grid::IsCubeOutOfBounds()
{
    bool result = CheckVerticalHasCube(GRID_WIDTH_SIZE - 3);
    if (result)
    {
        OnCubeTouchBoundEvent();
    }
    return result;
}
