#pragma once
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Game/Enum/EShape.h"

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

    FParticleProperty();

    FParticleProperty(const Vec2& velocity, float radius, const Vec2& position, const Rgba8& color, EShape shape);

    FParticleProperty(const Vec2& position, const Rgba8& color, int num_debris, const Vec2& average_velocity,
                      float max_scatter_speed, float average_radius, float minOpacity, float maxOpacity, EShape shape);
};
