#pragma once
#include "Game/Entity/Cube.hpp"

class BaseTetromino
{
public:
    virtual ~BaseTetromino();
    Cube*   m_cubes[4][2]  = {{nullptr}};
    bool    m_layout[4][2] = {{false}};

    BaseTetromino() = delete;

    BaseTetromino(IntVec2 startPos, Grid* parentGrid);
    BaseTetromino(IntVec2 startPos);

    virtual BaseTetromino* InitTetromino();
    /**
     * Remove the specific cube pointer in Tetromino
     * @param cube Target Cube pointer you want to remove in Tetromino
     * @return whether or not remove success
     */
    virtual bool RemoveCubePointerInTetromino(Cube* cube);

    virtual bool MarkCubeAsGarbage(Cube* cube);

    virtual void SetParentGrid(Grid* parentGrid);
    // DANGEROUS
    virtual Cube* GetChildCubes();

    virtual void SetChildCubesStatic();

    // GamePlay
    virtual bool IsAllCubeDie();

    virtual int GetNumCubesDefined();

    /**
     * Set the Tetromino delta position, with check
     * @param deltaPos The deltaPos you want Tetromino and its child cube move
     * @return Whether or not success move
     */
    virtual bool SetDeltaPosition(IntVec2 deltaPos);

private:
    Grid* m_parentGrid = nullptr;

    IntVec2 m_posGrid = IntVec2(-1, -1);
};
