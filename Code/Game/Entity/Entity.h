#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Vec2.hpp"

class Game;
/*
a.	(1) Entity is an abstract base class (ABC), i.e. is never directly instantiated (e.g. no “new Entity”)
b.	(1) PlayerShip, Bullet, and Asteroid all derive (inherit) from Entity
c.	(1) Entity provides base virtual implementations of each basic action (Update, Render, Die)
d.	(1) Entity’s constructor takes a pointer to the Game instance, and a starting position
e.	(2) Entity provides common inquiries/accessors (IsOffscreen, GetForwardNormal, IsAlive, etc.)
f.	(2) Holds several universal data members used by most/all entities, including (at minimum):
*/
class Entity
{
public:
    virtual ~Entity();
    Entity(Game* owner, const Vec2& startPosition, float orientationDegree);
    // Pure virtual
    virtual void Update(float deltaSeconds) = 0;
    virtual void Render() const = 0;
    virtual void InitializeLocalVerts() = 0;
    virtual void DebugRender() const;

    virtual void Die();

    /**
     * A method that handle custom logic for entity, the collision check
     * is handle by Game, and the entity could implement the method to handle
     * custom OnCollied
     * @param other Other Entity that the entity collied with
     */
    virtual void OnColliedEnter(Entity* other);

    bool IsOffscreen() const;
    virtual bool IsAlive() const;
    Vec2 GetForwardNormal() const;

    virtual int& GetHealth() { return m_health; }

    bool IsGarbage() const { return m_isGarbage; }

public:
    Vec2 m_position; // the Entity’s 2D (x,y) Cartesian origin/center location, in world space
    Vec2 m_velocity; // the Entity’s linear 2D (x,y) velocity, in world units per second
    float m_orientationDegrees = 0.f; // its forward angle, in degrees (counter-clockwise from +x/east)
    float m_angularVelocity = 0.f; // the Entity’s signed angular velocity (spin rate), in degrees per second
    float m_physicsRadius = 5.f; // the Entity’s (inner, conservative) disc-radius for all physics purposes
    float m_cosmeticRadius = 10.f; // the Entity’s (outer, liberal) disc-radius that encloses all of its vertexes

    bool m_isDead = false; // whether the Entity is “dead” in the game; affects entity and game logic
    bool m_isGarbage = false; // whether the Entity should be deleted at the end of Game::Update()
    Game* m_game = nullptr; // a pointer back to the Game instance

    Rgba8 m_color;

    float m_ageInSeconds = 0.f;

    // TODO: I know how to do getter and setter but I don't want do
public:
    int m_health = 1; // how many “hits” the entity can sustain before dying
};
