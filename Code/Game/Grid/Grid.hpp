#pragma once
#include "Game/GameCommon.hpp"
#include "Game/Entity/Cube.hpp"

struct IntVec2;

class Grid
{
public:
    Grid(Game* game);
    ~Grid();

    Cube* PlaceCube(IntVec2 gridPos);

    float GetHorizontalBaseLine(int horizontalIndex);

    // Events
    void OnCubeTouchBaseLineEvent(Cube* cube);

private:
    int m_width  = GRID_WIDTH_SIZE;
    int m_height = GRID_HEIGHT_SIZE;

    Game* m_game;

    Cube* m_cubesData[GRID_HEIGHT_SIZE][GRID_WIDTH_SIZE] = {{nullptr}};

public:
    static Vec2    GetPositionFromGrid(const IntVec2& gridPos);
    static IntVec2 GetGridPositionFromPosition(const Vec2& position);
};
