#pragma once
#include "Game/Entity/Entity.h"
/*
a.	(1) Derives (inherits) from Entity (e.g. class PlayerShip : public Entity)
b.	(1) The only player-controlled Entity
c.	(3) Turns (at 300 degrees per second) while S or F key is held (but not if both held)
d.	(3) Accelerates (30 world units/second/second) in its forward direction while E key is held
e.	(2) Fires a bullet (at its “nose”, facing/moving in its forward direction) when space is pressed; does NOT fire multiple bullets if space is held down
f.	(3) Dies (but does not get deleted!) if it touches an Asteroid
g.	(2) Respawns (at screen center, facing right/east, with zero velocity) if N is pressed while dead
h.	(1) Has a physics radius of 1.75 (used for all collision detections), and a cosmetic radius of 2.25
i.	(2) Bounces off world edges; physics disc is snapped in-bounds, and x (or y) velocity is reversed
j.	(2) Owns a vertex array of 5 triangles (15 vertexes) stored in local space; see Appendix A
        …and by “local space” here we mean: facing +x/east, centered about (0,0) at the object’s origin
k.	(2) In PlayerShip::Render, a (temporary, local) duplicate copy of this vertex array is created, then:
l.	(1) A TransformVertexArrayXY3D() utility function rotates & translates the temp array (from “local space” into “world space”) before drawing:
*/
//
constexpr int NUM_SHIP_TRIS  = 5;
constexpr int NUM_SHIP_VERTS = 3 * NUM_SHIP_TRIS;

class PlayerShip : public Entity
{
public:
    /**
     * 
     * @param gameInstance 
     * @param startPosition 
     */
    PlayerShip(Game* gameInstance, const Vec2& startPosition, float oritentationDeg);
    ~PlayerShip() override;

    void Update(float deltaSeconds) override;
    void Render() const override;

    void Die() override;

    void InitializeLocalVerts() override;

    void Respawn();

    void OnColliedEnter(Entity* other) override;

private:
    void UpdateFromInputSystem(float& deltaSeconds);
    void UpdateFromKeyBoard(float& deltaSeconds);
    void UpdateFromController(float& deltaSeconds);
    void BounceOffWalls();

    void StartAction(int index);

    Vertex_PCU m_localVerts[NUM_SHIP_VERTS];
    bool       m_isTurningLeft  = false;
    bool       m_isTurningRight = false;
    bool       m_isThrusting    = false;
    float      m_thrustRate     = 0.0f;

    float m_particleTimer = 0.f;
    float m_actionTimer   = 0.f;
};
