#pragma once
#include "Entity.h"
#include "Engine/Math/IntVec2.hpp"

class AABB2;
class BaseTetromino;
class Grid;

class Cube : public Entity
{
public:
    Cube(Game* owner, const Vec2& startPosition, float orientationDegree);
    Cube(Game* owner, Grid* grid, const Vec2& startPosition, float orientationDegree);
    Cube(Game* owner, Grid* grid, const IntVec2& startPosition, float orientationDegree);
    Cube();
    ~Cube() override;
    void Update(float deltaSeconds) override;
    void Render() const override;
    void InitializeLocalVerts() override;
    void DebugRender() const override;
    void Die() override;
    void OnColliedEnter(Entity* other) override;
    bool IsAlive() const override;

    bool           SetCubeStatic();
    void           UpdateGridPosition();
    void           CorrectWorldPosition();
    void           SetParentTetromino(BaseTetromino* parentTetromino);
    BaseTetromino* GetParentTetromino() const;

    AABB2*  m_aabb;
    IntVec2 m_gridPos;

    // the relative position in Tetromino
    IntVec2 m_TetrominoPosition = IntVec2(0, 0);

    bool m_EnablePhysicalRadius = false;
    bool m_IsStatic             = false;

private:
    Grid*          m_parentGrid      = nullptr;
    BaseTetromino* m_parentTetromino = nullptr;
};
