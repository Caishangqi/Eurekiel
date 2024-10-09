#pragma once

#include "Game/Entity/Entity.h"

constexpr int NUM_WASP_VERTS = 6;

class Wasp : public Entity
{
public:
    Wasp(Game* owner, const Vec2& startPosition, float orientationDegree);

    ~Wasp() override;

    void Die() override;

    void InitializeLocalVerts() override;

    void Render() const override;

    void Update(float deltaSeconds) override;

    void OnColliedEnter(Entity* other) override;

private:
    Vertex_PCU m_localVerts[NUM_WASP_VERTS];
};
