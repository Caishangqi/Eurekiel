#pragma once
#include "Game/Entity/Entity.h"
/*
a.	(2) Spawned at the PlayerShip’s nose whenever space bar is pressed, facing & moving ship-forward
b.	(1) Moves forward (drifts) in a straight line (at 50 world units/second) until it dies
c.	(1) Faces the direction in which it moves (e.g. the long “tail” points backwards)
d.	(1) Is rendered as a vertex array of 2 triangles (6 vertexes); see Appendix A
e.	(1) Has a physics radius of 0.5 (used for all collision detections), and a cosmetic radius of 2.0
f.	(2) Rendered in the same way as the PlayerShip (vertexes stored as a local-space vertex array, use TransformVertexArray to rotate & translate a temp copy, then DrawVertexArray to render)
g.	(2) Dies if its m_cosmetic radius disc goes entirely offscreen
h.	(2) Dies if it overlaps an Asteroid (using each Entity’s m_physicsRadius disc)
i.	(1) Maximum of 20 Bullets alive at once; attempting to spawn an 21st Bullet opens an ERROR_RECOVERABLE dialogue and refuses to spawn (note: should be difficult to achieve at normal speed, but much easier in slow-motion)
*/
//
constexpr int NUM_BULLET_TRIS = 2;
constexpr int NUM_BULLETS_VERTS = 3 * NUM_BULLET_TRIS;

class Bullet : public Entity
{
public:
    Bullet(Game* owner, const Vec2& startPosition, float oreintationDegrees);
    ~Bullet() override;

    virtual void Update(float deltaTime) override;
    virtual void Render() const override;
    virtual void InitializeLocalVerts() override;

    void OnColliedEnter(Entity* other) override;

private:
    Vertex_PCU m_localVerts[NUM_BULLETS_VERTS];
};
