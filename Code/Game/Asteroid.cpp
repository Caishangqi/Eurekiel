#include "Asteroid.h"
#include "GameCommon.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

Asteroid::Asteroid(Game* gameInstance, const Vec2& startPosition, float orientationDegrees): Entity(
    gameInstance, startPosition, orientationDegrees)
{
    m_health = 3;
    m_physicsRadius = ASTEROID_PHYSICS_RADIUS;
    m_cosmeticRadius = ASTEROID_COSMETIC_RADIUS;
    Asteroid::InitializeLocalVerts();
}

Asteroid::~Asteroid()
{
}

void Asteroid::Update(float deltaTime)
{
}

void Asteroid::Render() const
{
}

// 14:32    https://smu.instructure.com/media_attachments_iframe/9925327?type=video&embedded=true
void Asteroid::InitializeLocalVerts()
{
    // Precompute random radii along each triangle-seam (at each outer vertex)
    float asteroidRadii[NUM_ASTEROID_SIDES] = {};
    for (int sideNum = 0; sideNum < NUM_ASTEROID_SIDES; ++sideNum)
    {
        asteroidRadii[sideNum] = g_rng->RollRandomFloatInRange(m_physicsRadius, m_cosmeticRadius);
    }

    // Precompute 2D vertex offsets
    constexpr float degreesPerAsterSide = 360.0f / (float)NUM_ASTEROID_SIDES;
    Vec2 asteroidLocalVertPositions[NUM_ASTEROID_SIDES] = {};
    for (int sideNum = 0; sideNum < NUM_ASTEROID_SIDES; ++sideNum)
    {
        float degrees = degreesPerAsterSide * (float)sideNum;
        float radius = asteroidRadii[sideNum];
        asteroidLocalVertPositions[sideNum].x = radius * CosDegrees(degrees);
        asteroidLocalVertPositions[sideNum].y = radius * SinDegrees(degrees);
    }

    // Build triangles
    for (int triNum = 0; triNum < NUM_ASTEROID_TRIS; ++triNum)
    {
        int startRadiusIndex = triNum;
        int endRadiusIndex = (triNum + 1) % NUM_ASTEROID_SIDES;
        int firstVertIndex = (triNum * 3) + 0;
        int secondVertIndex = (triNum * 3) + 1;
        int thirdVertIndex = (triNum * 3) + 2;
        Vec2 secondVertOfs = asteroidLocalVertPositions[startRadiusIndex];
        Vec2 thirdVertOfs = asteroidLocalVertPositions[endRadiusIndex];
        m_localVerts[firstVertIndex].m_position = Vec3(0.f, 0.f, 0.f);
        m_localVerts[secondVertIndex].m_position = Vec3(secondVertOfs.x, secondVertOfs.y, 0.f);
        m_localVerts[thirdVertIndex].m_position = Vec3(thirdVertOfs.x, thirdVertOfs.y, 0.f);
    }

    // Set colors
    for (int vertIndex = 0; vertIndex < NUM_ASTEROID_VERTS; ++vertIndex)
    {
        m_localVerts[vertIndex].m_color = Rgba8(100, 100, 100);
    }
}
