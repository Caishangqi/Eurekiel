#include "Entity.h"
#include <cstdlib>

#include "Game.hpp"

Entity::~Entity()
{
    /*free(m_vertices);
    m_vertices = nullptr;*/

    delete m_game;
    m_game = nullptr;
}

Entity::Entity(Game* owner, const Vec2& startPosition, float orientationDegree)
{
    m_game = owner;
    m_position = startPosition;
    m_orientationDegrees = orientationDegree;
}

void Entity::Update(float deltaSeconds)
{
}


void Entity::DebugRender() const
{
}

void Entity::Die()
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
    return false;
}

Vec2 Entity::GetForwardNormal()
{
    return Vec2::MakeFromPolarDegrees(m_orientationDegrees, 1);
}
