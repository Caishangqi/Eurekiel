#pragma once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Game/Entity/Entity.h"
#include "Game/Enum/EShape.h"

enum EShape : int;
struct FParticleProperty;
class BaseParticle;
struct Rgba8;
struct Vec2;

class ParticleHandler
{
protected:
    ParticleHandler();
    static ParticleHandler* instance_;
    BaseParticle*           m_baseParticle[1024] = {};

public:
    ParticleHandler(const ParticleHandler& other)             = delete;
    void                    operator=(const ParticleHandler&) = delete;
    static ParticleHandler* getInstance();
    void                    Update(float deltaTime);
    void                    Render();
    void                    HandleParticleCollision();
    void                    GarbageCollection();
    /**
     * Clean all particles in the current scene
     */
    void CleanParticle();

    void SpawnNewParticleCluster(FParticleProperty property);

    void SpawnNewParticle(FParticleProperty property);
};

struct FParticleProperty
{
    // Individual
    Vec2  velocity;
    float radius;
    float lifeTime;
    float angularVelocity;
    float orientationDegrees;

    // Common
    Vec2   position; //
    Rgba8  color; //
    EShape shape;

    bool fadeOpacity = false;

    // Cluster
    int   numDebris;
    Vec2  averageVelocity;
    float maxScatterSpeed;
    float averageRadius;

    float minLifeTime = 0.1f;
    float maxLifeTime = 0.1f;

    float minAngularVelocity = 0.1f;
    float maxAngularVelocity = 0.1f;

    float minOpacity = 0.1f;
    float maxOpacity = 1.0f;

    FParticleProperty(): radius(1), lifeTime(1.f), angularVelocity(0), orientationDegrees(0), shape(PIXEL),
                         fadeOpacity(true),
                         numDebris(1),
                         maxScatterSpeed(10.f), averageRadius(10.f), minOpacity(0.1f), maxOpacity(1.0f)
    {
    }

    FParticleProperty(const Vec2& velocity, float radius, const Vec2& position, const Rgba8& color, EShape shape)
        : velocity(velocity),
          radius(radius),
          position(position),
          color(color), shape(shape)
    {
    }

    FParticleProperty(const Vec2& position, const Rgba8& color, int num_debris, const Vec2& average_velocity,
                      float max_scatter_speed, float average_radius, float minOpacity, float maxOpacity, EShape shape)
        : radius(0), lifeTime(0), angularVelocity(0), orientationDegrees(0), position(position),
          color(color),
          shape(shape),
          fadeOpacity(true),
          numDebris(num_debris),
          averageVelocity(average_velocity), maxScatterSpeed(max_scatter_speed), averageRadius(average_radius),
          minOpacity(minOpacity), maxOpacity(maxOpacity)
    {
    }
};
