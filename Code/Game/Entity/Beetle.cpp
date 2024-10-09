#include "Beetle.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/Game.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Particle/ParticleHandler.hpp"
#include "Game/Resource/SoundRes.hpp"

/*
 *  Spawns at a random position just offscreen (using the Beetle’s cosmetic radius)
	(1) Always faces the player (perfectly/instantly)
	(1) Moves forward at a constant speed (around 5-12 units per second, your choice)
	(1) Has a distinct look (shape and color), different from all other entities
	(1) Kills/damages the player on touch; bullets deal 1 damage; Beetle has N (> 1) health.

 */

Beetle::Beetle(Game* owner, const Vec2& startPosition, float orientationDegree): Entity(
    owner, startPosition, orientationDegree)
{
    m_color          = Rgba8(100, 160, 60);
    m_health         = BEETLE_HEALTH;
    m_velocity       = Vec2::MakeFromPolarDegrees(m_orientationDegrees, BEETLE_SPEED);
    m_cosmeticRadius = BEETLE_COSMETIC_RADIUS;
    m_physicsRadius  = BEETLE_PHYSICS_RADIUS;
    Beetle::InitializeLocalVerts();
}

Beetle::~Beetle()
{
}

void Beetle::Die()
{
    Entity::Die();
    FParticleProperty pp;
    pp.fadeOpacity        = true;
    pp.numDebris          = 64;
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

    ParticleHandler::getInstance()->SpawnNewParticleCluster(pp);
    g_theAudio->StartSound(SOUND::ENEMY_DIES);
}


void Beetle::Update(float deltaSeconds)
{
    if (m_game->m_PlayerShip != nullptr && m_game->m_PlayerShip->IsAlive())
    {
        //Update AI logic
        Vec2 facingDirection = m_game->m_PlayerShip->m_position - this->m_position;
        m_orientationDegrees = facingDirection.GetOrientationDegrees();
        // Vector towards player
        m_velocity = BEETLE_SPEED * facingDirection.GetNormalized();
    }

    m_position += m_velocity * deltaSeconds;

    // Life management
    if (m_health <= 0)
        m_isDead = true;

    if (m_isDead)
        m_isGarbage = true;
}

void Beetle::Render() const
{
    if (!m_isDead)
    {
        Vertex_PCU tempWorldVerts[NUM_BEETLE_VERTS];
        for (int vertIndex = 0; vertIndex < NUM_BEETLE_VERTS; vertIndex++)
        {
            tempWorldVerts[vertIndex] = m_localVerts[vertIndex];
        }
        TransformVertexArrayXY3D(NUM_BEETLE_VERTS, tempWorldVerts, 1.f, m_orientationDegrees, m_position);
        g_renderer->DrawVertexArray(NUM_BEETLE_VERTS, tempWorldVerts);
    }
    DebugRender();
}

void Beetle::InitializeLocalVerts()
{
    m_localVerts[0].m_position = Vec3(1.f, 1.f, 0.f);
    m_localVerts[1].m_position = Vec3(-2.f, 2.f, 0.f);
    m_localVerts[2].m_position = Vec3(-2.f, -2.f, 0.f);

    m_localVerts[3].m_position = Vec3(1.f, 1.f, 0.f);
    m_localVerts[4].m_position = Vec3(-2.f, -2.f, 0.f);
    m_localVerts[5].m_position = Vec3(1.f, -1.f, 0.f);

    for (int vertIndex = 0; vertIndex < NUM_BEETLE_VERTS; vertIndex++)
    {
        m_localVerts[vertIndex].m_color = m_color;
    }
}

void Beetle::OnColliedEnter(Entity* other)
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
