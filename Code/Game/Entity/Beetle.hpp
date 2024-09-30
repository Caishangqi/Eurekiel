#pragma once
#include "Game/Entity/Entity.h"

constexpr int NUM_BEETLE_VERTS = 6;

class Beetle : public Entity
{
public:
    Beetle(Game* owner, const Vec2& startPosition, float orientationDegree);

    ~Beetle() override;
void Die() override;
    void Update(float deltaSeconds) override;
    void Render() const override;
    void InitializeLocalVerts() override;
    void OnColliedEnter(Entity* other) override;

private:
    Vertex_PCU m_localVerts[NUM_BEETLE_VERTS];
};
