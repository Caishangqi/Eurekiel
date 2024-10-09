#include "Wasp.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Particle/ParticleHandler.hpp"
#include "Game/Resource/SoundRes.hpp"

Wasp::Wasp(Game* owner, const Vec2& startPosition, float orientationDegree): Entity(
    owner, startPosition, orientationDegree)
{
    m_color          = Rgba8(255, 255, 60);
    m_health         = WASP_HEALTH;
    m_velocity       = Vec2::MakeFromPolarDegrees(m_orientationDegrees, WASP_SPEED);
    m_cosmeticRadius = WASP_COSMETIC_RADIUS;
    m_physicsRadius  = WASP_PHYSICS_RADIUS;
    Wasp::InitializeLocalVerts();
}

Wasp::~Wasp()
{
}

void Wasp::Die()
{
    Entity::Die();
    FParticleProperty pp;
    pp.fadeOpacity        = true;
    pp.numDebris          = 60;
    pp.averageVelocity    = m_velocity;
    pp.maxScatterSpeed    = 40.f;
    pp.color              = m_color;
    pp.position           = m_position;
    pp.minAngularVelocity = 0.f;
    pp.maxAngularVelocity = 0.f;

    pp.minOpacity = 0.6f;
    pp.maxOpacity = 1.0f;

    pp.minLifeTime = 2.0f;
    pp.maxLifeTime = 3.0f;

    g_theAudio->StartSound(SOUND::ENEMY_DIES);
    ParticleHandler::getInstance()->SpawnNewParticleCluster(pp);
}

void Wasp::InitializeLocalVerts()
{
    m_localVerts[0].m_position = Vec3(0.f, 2.f, 0.f);
    m_localVerts[1].m_position = Vec3(2.f, 0.f, 0.f);
    m_localVerts[2].m_position = Vec3(0.f, -2.f, 0.f);

    m_localVerts[3].m_position = Vec3(0.f, 1.f, 0.f);
    m_localVerts[4].m_position = Vec3(0.f, -1.f, 0.f);
    m_localVerts[5].m_position = Vec3(-2.f, 0.f, 0.f);

    for (int vertIndex = 0; vertIndex < NUM_WASP_VERTS; vertIndex++)
    {
        m_localVerts[vertIndex].m_color = Rgba8(255, 255, 60);
    }
}

void Wasp::Render() const
{
    if (!m_isDead)
    {
        Vertex_PCU tempWorldVerts[NUM_WASP_VERTS];
        for (int vertIndex = 0; vertIndex < NUM_WASP_VERTS; vertIndex++)
        {
            tempWorldVerts[vertIndex] = m_localVerts[vertIndex];
        }
        TransformVertexArrayXY3D(NUM_WASP_VERTS, tempWorldVerts, 1.f, m_orientationDegrees, m_position);
        g_renderer->DrawVertexArray(NUM_WASP_VERTS, tempWorldVerts);
    }
    DebugRender();
}

void Wasp::Update(float deltaSeconds)
{
    if (m_game->m_PlayerShip->IsAlive())
    {
        //Update AI logic
        Vec2 facingDirection = m_game->m_PlayerShip->m_position - this->m_position;
        m_orientationDegrees = facingDirection.GetOrientationDegrees();
        // Vector towards player
        Vec2 acceleration = facingDirection.GetNormalized() * WASP_ACCELERATION;
        m_velocity += acceleration * deltaSeconds;
    }
    m_velocity.ClampLength(WASP_SPEED_MAX);
    m_position += m_velocity * deltaSeconds;

    // Life management
    if (m_health <= 0)
    {
        m_isDead = true;
        Die();
    }


    if (m_isDead)
        m_isGarbage = true;
}

void Wasp::OnColliedEnter(Entity* other)
{
    Entity::OnColliedEnter(other);
    m_health--;

    FParticleProperty pp;
    pp.fadeOpacity        = true;
    pp.numDebris          = 20;
    pp.averageVelocity    = m_velocity + other->m_velocity;
    pp.maxScatterSpeed    = 20.f;
    pp.color              = m_color;
    pp.position           = m_position;
    pp.minAngularVelocity = 0.f;
    pp.maxAngularVelocity = 0.f;

    pp.minOpacity = 0.7f;
    pp.maxOpacity = 1.0f;

    pp.minLifeTime = .5f;
    pp.maxLifeTime = 1.5f;
    g_theAudio->StartSound(SOUND::ENEMY_DAMAGED);
    ParticleHandler::getInstance()->SpawnNewParticleCluster(pp);
}
