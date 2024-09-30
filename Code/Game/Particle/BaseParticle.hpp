#pragma once
#include "Game/Entity/Entity.h"

class ParticleHandler;

class BaseParticle
{
public:
    // Individual
    Vec2 m_velocity;
    float m_radius;
    float m_orientationDegrees = 0.f;

    // Common
    Vec2 m_position; //
    Rgba8 m_color; //
    bool m_fadeOpacity = true;

public:
    BaseParticle(float lifetime, Vec2 position, Vec2 velocity, float radius, Rgba8 color, float angularVelocity,
                 float orientationDegrees, bool fadeOpacity,
                 ParticleHandler* particleHandler);

    BaseParticle(ParticleHandler* particleHandler);

    ~BaseParticle();
    
    void Update(float deltaTime);
    void Render();

    bool IsAlive();
    bool IsGarbage();

private:
    void InitializeLocalVerts();

private:
    bool m_isDead = false;
    bool m_isGarbage = false;
    ParticleHandler* m_particleHandler = nullptr;
    float m_angularVelocity = 0.f;
    float m_lifetime = 1.0f;
    Vertex_PCU m_localVerts[6];
    float m_lifetimeInitial = 0.f;
};
