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
     * 
     * @param cube Target Cube pointer you want to remove in Tetromino
     * @return whether or not remove success
     */
    virtual bool RemoveCubePointerInTetromino(Cube* cube);

    virtual bool MarkCubeAsGarbage(Cube* cube);

    virtual void SetParentGrid(Grid* parentGrid);
    // DANGEROUS
    virtual Cube* GetChildCubes();

    virtual void SetChildCubesStatic();

private:
    Grid* m_parentGrid = nullptr;

    IntVec2 m_posGrid = IntVec2(-1, -1);
};
