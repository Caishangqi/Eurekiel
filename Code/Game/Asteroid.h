#pragma once
#include "Entity.h"
/*
a.	(1) 6 Asteroids are spawned at start of a new Game (also, see the ‘I’ debug cheat key, below)
b.	(2) Spawned at a random position onscreen
c.	(3) Spawned with random orientation & random velocity (but with fixed speed = 10 units/second)
d.	(1) Moves forward (drifts) in a straight line (at 10 world units/second) until it dies
e.	(3) Spins while it drifts, at a constant angular velocity, chosen randomly within [-200,200] at birth
f.	(1) Has a physics radius of 1.6 (used for all collision detections), and a cosmetic radius of 2.0
g.	(2) Is rendered as a vertex array of 16 triangles (48 vertexes; see Appendix A)
i.	Each asteroid’s shape is randomly generated and unique; the “radius” of each vertex is randomly chosen between
    the asteroid’s inner (physics) radius and outer (cosmetic) radius.
ii.	Each asteroid retains its random-generated unique shape once created (i.e. don’t re-randomize vertex radii
    every frame!)
h.	(2) Rendered in the same fashion as the PlayerShip (vertexes stored as a local-space vertex array, use
    TransformVertexArray to rotate & translate, DrawVertexArray to render)
i.	(1) Takes 1 damage if it overlaps a Bullet or Player (using each Entity’s m_physicsRadius disc)
j.	(2) Dies if it takes 3+ points of damage
k.	(1) Dies if its m_cosmetic radius disc goes entirely offscreen
l.	(2) Maximum of 12 Asteroids alive at once; attempting to spawn a 13th Asteroid opens an ERROR_RECOVERABLE
        dialogue and refuses to spawn
*/
//
constexpr int NUM_ASTEROID_SIDES = 16;
constexpr int NUM_ASTEROID_TRIS = NUM_ASTEROID_SIDES;
constexpr int NUM_ASTEROID_VERTS = 3 * NUM_ASTEROID_TRIS;

class Asteroid : public Entity
{
public:
    Asteroid(Game* gameInstance, const Vec2& startPosition, float orientationDegrees);
    ~Asteroid() override;

    virtual void Update(float deltaTime) override;
    virtual void Render() const override;

    virtual void InitializeLocalVerts() override;

private:
    Vertex_PCU m_localVerts[NUM_ASTEROID_VERTS];
};
