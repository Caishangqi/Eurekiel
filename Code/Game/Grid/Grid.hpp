#pragma once
#include "Game/GameCommon.hpp"
struct TetrominoRequestOperateEvent;
struct CubeTouchBaseLineEvent;
class BaseTetromino;
class Cube;
class Game;
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

    BaseTetromino* m_controledTetromino = nullptr; // Must Ensure the whole grid have only one that can control

    void ResetGrid();
    void ClearTetromino();

    // Events
    void OnCubeTouchBaseLineEvent(std::shared_ptr<CubeTouchBaseLineEvent> event);
    void OnCubeCancelEvent(int verticalIndex);
    void OnCubeTouchBoundEvent();

    // Events
    void OnCubeDieEvent(Cube* cube);
    void OnTetrominoDieEvent(BaseTetromino* tetromino);
    void OnTetrominoRequestOperateEvent(std::shared_ptr<TetrominoRequestOperateEvent> event);

private:
    int m_width  = GRID_WIDTH_SIZE;
    int m_height = GRID_HEIGHT_SIZE;

    Game* m_game;

    BaseTetromino* m_TetrominoData[256] = {nullptr};

    bool m_NeedCancelCubes = false;

public:
    static Vec2    GetPositionFromGrid(const IntVec2& gridPos);
    static IntVec2 GetGridPositionFromPosition(const Vec2& position);
    static Vec2    ConvertGridPositionToWorldPosition(const IntVec2& gridPos);

    /**
     * Remove correspond cube pointer in the grid system maintained array
     * Note: YOU MUST SET CORRESPOND CUBE m_IsGarbage TRUE before you
     * null the pointer 
     * @param cube Target Cube pointer you want to remove in Tetromino
     * @return whether or not remove success
     */
    bool RemoveCubePointerInGrid(Cube* cube);
    /**
     * Remove all cubes pointers in a vertical line of grid system maintained
     * array
     * Note: YOU MUST SET CORRESPOND CUBE m_IsGarbage TRUE before you
     * null the pointer 
     * @param verticalIndex Target vertical index you want to null out
     * @return whether or not remove success
     */
    bool RemoveVerticalFullPointer(int verticalIndex);

    /**
     * Make all cubes as garbage in a specific vertical line of grid system
     * maintained array
     * Note: YOU MUST NULL THE POINTER IN THE GRID SYSTEM after you make cube
     * as garbage
     * @param verticalIndex Target vertical index you want to null out
     * @return whether or not remove success
     */
    bool MarkVerticalFullGarbage(int verticalIndex);

    bool CheckVerticalFull(int verticalIndex);

    bool CheckVerticalHasCube(int verticalIndex);

    Cube* GetVerticalFullCubes(int verticalIndex);

    void MoveVerticalCubes(int targetVerticalIndex, int fromVerticalIndex);

    int GetMostNearEmptyVerticalIndex();

    void MoveCubesRightOfEmptyColumnLeft(int emptyColumnIndex);

    /**
     * 
     * @return Whether or not cube out of bound
     */
    bool IsCubeOutOfBounds();

    Cube* m_cubesData[GRID_HEIGHT_SIZE][GRID_WIDTH_SIZE] = {{nullptr}};
};
