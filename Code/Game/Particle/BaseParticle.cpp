#include "BaseParticle.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/GameCommon.hpp"


BaseParticle::BaseParticle(float            lifetime, Vec2         position, Vec2 velocity, float radius, Rgba8 color,
                           float            angularVelocity, float orientationDegrees, bool fadeOpacity,
                           ParticleHandler* particleHandler)
{
    m_position           = position;
    m_velocity           = velocity;
    m_radius             = radius;
    m_color              = color;
    m_particleHandler    = particleHandler;
    m_angularVelocity    = angularVelocity;
    m_orientationDegrees = orientationDegrees;
    m_lifetime           = lifetime;
    m_lifetimeInitial    = m_lifetime;
    m_fadeOpacity        = fadeOpacity;
    InitializeLocalVerts();
}

BaseParticle::BaseParticle(ParticleHandler* particleHandler): m_radius(0)
{
    m_particleHandler = particleHandler;
}

BaseParticle::~BaseParticle()
{
    m_particleHandler = nullptr;
}

void BaseParticle::Update(float deltaTime)
{
    m_orientationDegrees += deltaTime * m_angularVelocity;
    m_position += m_velocity * deltaTime;

    m_lifetime -= deltaTime;
    if (m_lifetime < 0.0f)
        m_isDead = true;

    if (m_isDead)
        m_isGarbage = true;
}


void BaseParticle::Render()
{
    if (!m_isDead)
    {
        Vertex_PCU tempWorldVerts[6];
        for (int vertIndex = 0; vertIndex < 6; vertIndex++)
        {
            tempWorldVerts[vertIndex] = m_localVerts[vertIndex];
        }
        if (m_fadeOpacity)
        {
            for (Vertex_PCU& temp_world_vert : tempWorldVerts)
            {
                temp_world_vert.m_color.a = static_cast<unsigned char>(static_cast<float>(temp_world_vert.m_color.a) * (
                    m_lifetime / m_lifetimeInitial));
            }
        }
        TransformVertexArrayXY3D(6, tempWorldVerts, 1.f, m_orientationDegrees, m_position);
        g_renderer->DrawVertexArray(6, tempWorldVerts);
    }
}

bool BaseParticle::IsAlive()
{
    return true;
}

bool BaseParticle::IsGarbage()
{
    return m_isGarbage;
}

void BaseParticle::InitializeLocalVerts()
{
    m_localVerts[0].m_position = Vec3(-.25f, -.25f, 0.f);
    m_localVerts[1].m_position = Vec3(.25f, .25f, 0.f);
    m_localVerts[2].m_position = Vec3(-.25f, .25f, 0.f);

    m_localVerts[3].m_position = Vec3(-.25f, -.25f, 0.f);
    m_localVerts[4].m_position = Vec3(.25f, .25f, 0.f);
    m_localVerts[5].m_position = Vec3(.25f, -.25f, 0.f);

    for (int vertIndex = 0; vertIndex < 6; vertIndex++)
    {
        m_localVerts[vertIndex].m_color = m_color;
    }
}
