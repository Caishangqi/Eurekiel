#include "FParticleProperty.hpp"

FParticleProperty::FParticleProperty(): radius(1), lifeTime(1.f), angularVelocity(0), orientationDegrees(0),
                                        shape(PIXEL),
                                        fadeOpacity(true),
                                        numDebris(1),
                                        maxScatterSpeed(10.f), averageRadius(10.f), minOpacity(0.1f), maxOpacity(1.0f)
{
}

FParticleProperty::FParticleProperty(const Vec2& velocity, float radius, const Vec2& position, const Rgba8& color,
                                     EShape      shape)
    : velocity(velocity),
      radius(radius),
      position(position),
      color(color), shape(shape)
{
}

FParticleProperty::FParticleProperty(const Vec2& position, const Rgba8&  color, int               num_debris,
                                     const Vec2& average_velocity, float max_scatter_speed, float average_radius,
                                     float       minOpacity, float       maxOpacity,
                                     EShape      shape)
    : radius(0), lifeTime(0), angularVelocity(0), orientationDegrees(0), position(position),
      color(color),
      shape(shape),
      fadeOpacity(true),
      numDebris(num_debris),
      averageVelocity(average_velocity), maxScatterSpeed(max_scatter_speed), averageRadius(average_radius),
      minOpacity(minOpacity), maxOpacity(maxOpacity)
{
}
