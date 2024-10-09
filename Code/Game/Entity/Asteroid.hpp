#pragma once
#include "Game/Entity/Entity.h"

constexpr int NUM_ASTEROID_SIDES = 16;
constexpr int NUM_ASTEROID_TRIS  = NUM_ASTEROID_SIDES;
constexpr int NUM_ASTEROID_VERTS = 3 * NUM_ASTEROID_TRIS;

class Asteroid : public Entity
{
public:
    Asteroid(Game* gameInstance, const Vec2& startPosition, float orientationDegrees);
    ~Asteroid() override;

    void Update(float deltaTime) override;
    void Render() const override;

    void Die() override;

    void InitializeLocalVerts() override;

    void OnColliedEnter(Entity* other) override;

private:
    Vertex_PCU m_localVerts[NUM_ASTEROID_VERTS];
};
