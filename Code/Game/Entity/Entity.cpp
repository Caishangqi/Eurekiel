#include "Game/Entity/Entity.h"


#include "Game/App.hpp"
#include "Game/Game.hpp"

Entity::Entity()
{
}

Entity::~Entity()
{
    /*free(m_vertices);
    m_vertices = nullptr;*/
    m_game = nullptr;
}

Entity::Entity(Game* owner, const Vec2& startPosition, float orientationDegree)
{
    m_game               = owner;
    m_position           = startPosition;
    m_orientationDegrees = orientationDegree;
}

void Entity::Update(float deltaSeconds)
{
}


void Entity::DebugRender() const
{
    if (g_theApp->m_isDebug)
    {
        // A dark grey (50,50,50) line is drawn to the PlayerShip from the center of each other entity
        // and then, for each entity
        DebugDrawLine(m_position, Vec2(WORLD_CENTER_X, WORLD_CENTER_Y), .2f, Rgba8(50, 50, 50));
        // its current forward vector is drawn in red (255,0,0), from center, length=cosmetic radius
        DebugDrawLine(m_position, m_position + GetForwardNormal() * m_cosmeticRadius, .2f, Rgba8(255, 0, 0));
        // its current relative-left vector is drawn in green (0,255,0) from center, same length
        DebugDrawLine(m_position, m_position + GetForwardNormal().GetRotated90Degrees() * m_cosmeticRadius, .2f,
                      Rgba8(0, 255, 0));
        // its cosmetic (outer) radius is drawn in magenta (255,0,255)
        DebugDrawRing(m_position, m_cosmeticRadius, .2f, Rgba8(255, 0, 255));
        // its physics (inner) radius is drawn in cyan (0,255,255)
        DebugDrawRing(m_position, m_physicsRadius, .2f, Rgba8(0, 255, 255));
        // its current m_velocity vector is drawn as a yellow (255,255,0) line segment
        DebugDrawLine(m_position, m_position + m_velocity, .2f, Rgba8(255, 255, 0));
    }
}

void Entity::Die()
{
    m_game->OnEntityDieEvent(this);
}

void Entity::OnColliedEnter(Entity* other)
{
}

bool Entity::IsOffscreen() const
{
    if (m_position.x > WORLD_SIZE_X + m_cosmeticRadius)
        return true;
    if (m_position.y > WORLD_SIZE_Y + m_cosmeticRadius)
        return true;
    if (m_position.x < -m_cosmeticRadius)
        return true;
    if (m_position.y < -m_cosmeticRadius)
        return true;
    return false;
}

bool Entity::IsAlive() const
{
    return !m_isDead;
}

Vec2 Entity::GetForwardNormal() const
{
    return Vec2::MakeFromPolarDegrees(m_orientationDegrees, 1);
}
