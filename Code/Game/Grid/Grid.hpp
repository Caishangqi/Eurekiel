#pragma once
#include "Game/GameCommon.hpp"
#include "Game/Entity/Cube.hpp"
#include "Game/Entity/Tetromino/BaseTetromino.hpp"

struct IntVec2;

class Grid
{
public:
    Grid(Game* game);
    ~Grid();

    void Update(float deltaSeconds);

    Cube*          PlaceCube(IntVec2 gridPos);
    BaseTetromino* PlaceTetromino(BaseTetromino* baseTetromino);
    float          GetHorizontalBaseLine(int horizontalIndex);

    // Events
    void OnCubeTouchBaseLineEvent(Cube* cube);

private:
    int m_width  = GRID_WIDTH_SIZE;
    int m_height = GRID_HEIGHT_SIZE;

    Game* m_game;


    BaseTetromino* m_TetrominoData[256] = {nullptr};

    bool m_NeedCancelCubes = false;

public:
    static Vec2    GetPositionFromGrid(const IntVec2& gridPos);
    static IntVec2 GetGridPositionFromPosition(const Vec2& position);

    /**
     * 
     * @param cube Target Cube pointer you want to remove in Tetromino
     * @return whether or not remove success
     */
    bool RemoveCubePointerInGrid(Cube* cube);

    bool CheckVerticalFull(int verticalIndex);

    bool RemoveVerticalFullPointer(int verticalIndex);

    bool MarkVerticalFullGarbage(int verticalIndex);

    Cube* GetVerticalFullCubes(int verticalIndex);

public:
    Cube* m_cubesData[GRID_HEIGHT_SIZE][GRID_WIDTH_SIZE] = {{nullptr}};
};
