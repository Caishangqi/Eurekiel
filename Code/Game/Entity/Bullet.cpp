#include "Bullet.h"

#include "Game/GameCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/Particle/ParticleHandler.hpp"

Bullet::Bullet(Game* owner, const Vec2& startPosition, float orientationDegrees): Entity(
    owner, startPosition, orientationDegrees)
{
    m_physicsRadius  = BULLET_PHYSICS_RADIUS;
    m_cosmeticRadius = BULLET_COSMETIC_RADIUS;

    m_health = 1;

    m_velocity = Vec2::MakeFromPolarDegrees(m_orientationDegrees, BULLET_SPEED);
    Bullet::InitializeLocalVerts();
}

Bullet::~Bullet()
{
    Entity::~Entity();
}

void Bullet::Update(float deltaTime)
{
    m_position += m_velocity * deltaTime;

    if (IsOffscreen())
        m_isDead = true;

    if (m_health <= 0)
    {
        m_isDead = true;
        Die();
    }

    if (m_isDead)
        m_isGarbage = true;
}

void Bullet::Render() const
{
    Vertex_PCU tempWorldVerts[NUM_BULLETS_VERTS];
    for (int vertIndex = 0; vertIndex < NUM_BULLETS_VERTS; vertIndex++)
    {
        tempWorldVerts[vertIndex] = m_localVerts[vertIndex];
    }
    TransformVertexArrayXY3D(NUM_BULLETS_VERTS, tempWorldVerts, 1.f, m_orientationDegrees, m_position);
    g_renderer->DrawVertexArray(NUM_BULLETS_VERTS, tempWorldVerts);
    DebugRender();
}

void Bullet::InitializeLocalVerts()
{
    // Local space Drawing
    m_localVerts[0].m_position = Vec3(0.5f, 0.0f, 0.0f);
    m_localVerts[1].m_position = Vec3(0.0f, 0.5f, 0.0f);
    m_localVerts[2].m_position = Vec3(0.0f, -0.5f, 0.0f);
    m_localVerts[0].m_color    = Rgba8(255, 255, 0, 255);
    m_localVerts[1].m_color    = Rgba8(255, 255, 0, 255);
    m_localVerts[2].m_color    = Rgba8(255, 255, 0, 255);

    m_localVerts[3].m_position = Vec3(0.0f, -0.5f, 0.0f);
    m_localVerts[4].m_position = Vec3(0.0f, 0.5f, 0.0f);
    m_localVerts[5].m_position = Vec3(-2.0f, 0.0f, 0.0f);
    m_localVerts[3].m_color    = Rgba8(255, 0, 0, 255);
    m_localVerts[4].m_color    = Rgba8(255, 0, 0, 255);
    m_localVerts[5].m_color    = Rgba8(255, 0, 0, 0);
}

void Bullet::OnColliedEnter(Entity* other)
{
    Entity::OnColliedEnter(other);
    m_health--;
    FParticleProperty pp;
    pp.fadeOpacity        = true;
    pp.numDebris          = 5;
    pp.averageVelocity    = m_velocity + other->m_velocity;
    pp.maxScatterSpeed    = 40.f;
    pp.color              = m_color;
    pp.position           = m_position;
    pp.minAngularVelocity = 0.f;
    pp.maxAngularVelocity = 0.f;

    pp.minOpacity = 0.8f;
    pp.maxOpacity = 1.0f;

    pp.minLifeTime = .5f;
    pp.maxLifeTime = .1f;

    ParticleHandler::getInstance()->SpawnNewParticleCluster(pp);
}
