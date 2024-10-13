#include "ParticleHandler.hpp"
#include "BaseParticle.hpp"
#include "FParticleProperty.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Game/GameCommon.hpp"

ParticleHandler* ParticleHandler::instance_ = nullptr;

ParticleHandler::ParticleHandler()
{
    printf("[particle]  Initialize particle system!\n");
}

ParticleHandler* ParticleHandler::getInstance()
{
    if (instance_ == nullptr)
    {
        instance_ = new ParticleHandler();
    }
    return instance_;
}

void ParticleHandler::Update(float deltaTime)
{
    for (BaseParticle* element : m_baseParticle)
    {
        if (element != nullptr)
            element->Update(deltaTime);
    }
    GarbageCollection();
}

void ParticleHandler::Render()
{
    for (BaseParticle* element : m_baseParticle)
    {
        if (element != nullptr && element->IsAlive())
            element->Render();
    }
}

void ParticleHandler::HandleParticleCollision()
{
}

void ParticleHandler::GarbageCollection()
{
    for (BaseParticle*& element : m_baseParticle)
    {
        if (element != nullptr && element->IsGarbage())
        {
            delete element;
            element = nullptr;
        }
    }
}

void ParticleHandler::CleanParticle()
{
    for (BaseParticle*& m_base_particle : m_baseParticle)
    {
        if (m_base_particle != nullptr)
        {
            delete m_base_particle;
            m_base_particle = nullptr;
        }
    }
}

void ParticleHandler::SpawnNewParticleCluster(FParticleProperty property
)
{
    // Common
    Vec2  position    = property.position;
    Rgba8 color       = property.color;
    bool  fadeOpacity = property.fadeOpacity;


    // Cluster
    int   numDebris       = property.numDebris;
    Vec2  averageVelocity = property.averageVelocity;
    float maxScatterSpeed = property.maxScatterSpeed;
    float averageRadius   = property.averageRadius;

    float minLifeTime = property.minLifeTime;
    float maxLifeTime = property.maxLifeTime;

    float minAngularVelocity = property.minAngularVelocity;
    float maxAngularVelocity = property.maxAngularVelocity;

    FParticleProperty prop;
    for (int number = 0; number < numDebris; ++number)
    {
        prop.fadeOpacity = property.fadeOpacity;
        prop.position    = position;
        prop.color       = color;

        prop.color.a = static_cast<unsigned char>(static_cast<float>(color.a) * g_rng->RollRandomFloatInRange(
            prop.minOpacity, prop.maxOpacity));

        prop.velocity = averageVelocity + Vec2(g_rng->RollRandomFloatInRange(-maxScatterSpeed, maxScatterSpeed),
                                               g_rng->RollRandomFloatInRange(-maxScatterSpeed, maxScatterSpeed));
        prop.radius          = g_rng->RollRandomFloatInRange(1.f, maxScatterSpeed);
        prop.angularVelocity = g_rng->RollRandomFloatInRange(minAngularVelocity, maxAngularVelocity);
        prop.lifeTime        = g_rng->RollRandomFloatInRange(minLifeTime, maxLifeTime);

        SpawnNewParticle(prop);
    }
    printf("[particle]         Spawn Particle Cluster at position: (%f, %f)\n", position.x, position.y);
}

void ParticleHandler::SpawnNewParticle(FParticleProperty property)
{
    Vec2  position           = property.position;
    Rgba8 color              = property.color;
    Vec2  velocity           = property.velocity;
    float radius             = property.radius;
    float lifeTime           = property.lifeTime;
    float angularVelocity    = property.angularVelocity;
    float orientationDegrees = property.orientationDegrees;
    bool  fadeOpacity        = property.fadeOpacity;


    for (BaseParticle*& element : m_baseParticle)
    {
        if (element == nullptr)
        {
            element = new BaseParticle(lifeTime, position, velocity, radius, color, angularVelocity, orientationDegrees,
                                       fadeOpacity,
                                       this);
            break;
        }
    }
}
