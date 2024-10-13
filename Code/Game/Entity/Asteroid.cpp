#include "Asteroid.hpp"
#include "Game/GameCommon.hpp"



Asteroid::Asteroid(Game* gameInstance, const Vec2& startPosition, float orientationDegrees): Entity(
    gameInstance, startPosition, orientationDegrees)
{
    m_color          = Rgba8(100, 100, 100);
    m_health         = 3;
    m_physicsRadius  = ASTEROID_PHYSICS_RADIUS;
    m_cosmeticRadius = ASTEROID_COSMETIC_RADIUS;


    m_angularVelocity = g_rng->RollRandomFloatInRange(-200.f, 200.f);
    m_velocity        = Vec2::MakeFromPolarDegrees(m_orientationDegrees, ASTEROID_SPEED);

    InitializeLocalVerts();
}

Asteroid::~Asteroid()
{
}

void Asteroid::Update(float deltaTime)
{
    m_orientationDegrees += deltaTime * m_angularVelocity;
    m_position += m_velocity * deltaTime;


    if (IsOffscreen())
    {
        // 水平方向包裹
        if (m_position.x > WORLD_SIZE_X + m_cosmeticRadius)
        {
            m_position.x = -m_cosmeticRadius;
        }
        else if (m_position.x < -m_cosmeticRadius)
        {
            m_position.x = WORLD_SIZE_X + m_cosmeticRadius;
        }

        // 垂直方向包裹
        if (m_position.y > WORLD_SIZE_Y + m_cosmeticRadius)
        {
            m_position.y = -m_cosmeticRadius;
        }
        else if (m_position.y < -m_cosmeticRadius)
        {
            m_position.y = WORLD_SIZE_Y + m_cosmeticRadius;
        }
    }


    if (m_health <= 0)
    {
        m_isDead = true;
        Die();
    }

    if (m_isDead)
        m_isGarbage = true;
}

void Asteroid::Render() const
{
    Vertex_PCU tempWorldVerts[NUM_ASTEROID_VERTS];
    for (int vertIndex = 0; vertIndex < NUM_ASTEROID_VERTS; vertIndex++)
    {
        tempWorldVerts[vertIndex] = m_localVerts[vertIndex];
    }
    TransformVertexArrayXY3D(NUM_ASTEROID_VERTS, tempWorldVerts, 1.f, m_orientationDegrees, m_position);
    g_renderer->DrawVertexArray(NUM_ASTEROID_VERTS, tempWorldVerts);
    DebugRender();
}

void Asteroid::Die()
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

    ParticleHandler::getInstance()->SpawnNewParticleCluster(pp);
}

// 14:32    https://smu.instructure.com/media_attachments_iframe/9925327?type=video&embedded=true
void Asteroid::InitializeLocalVerts()
{
    // TODO: Following code is directly stole from Squirrel, refactor after complete function implementation

    // Precompute random radii along each triangle-seam (at each outer vertex)
    float asteroidRadii[NUM_ASTEROID_SIDES] = {};
    for (int sideNum = 0; sideNum < NUM_ASTEROID_SIDES; ++sideNum)
    {
        asteroidRadii[sideNum] = g_rng->RollRandomFloatInRange(m_physicsRadius, m_cosmeticRadius);
    }

    // Precompute 2D vertex offsets
    constexpr float degreesPerAsterSide                            = 360.0f / static_cast<float>(NUM_ASTEROID_SIDES);
    Vec2            asteroidLocalVertPositions[NUM_ASTEROID_SIDES] = {};
    for (int sideNum = 0; sideNum < NUM_ASTEROID_SIDES; ++sideNum)
    {
        float degrees                         = degreesPerAsterSide * static_cast<float>(sideNum);
        float radius                          = asteroidRadii[sideNum];
        asteroidLocalVertPositions[sideNum].x = radius * cosf(degrees);
        asteroidLocalVertPositions[sideNum].y = radius * cosf(degrees);
    }

    // Build triangles
    for (int triNum = 0; triNum < NUM_ASTEROID_TRIS; ++triNum)
    {
        int  startRadiusIndex                    = triNum;
        int  endRadiusIndex                      = (triNum + 1) % NUM_ASTEROID_SIDES;
        int  firstVertIndex                      = (triNum * 3) + 0;
        int  secondVertIndex                     = (triNum * 3) + 1;
        int  thirdVertIndex                      = (triNum * 3) + 2;
        Vec2 secondVertOfs                       = asteroidLocalVertPositions[startRadiusIndex];
        Vec2 thirdVertOfs                        = asteroidLocalVertPositions[endRadiusIndex];
        m_localVerts[firstVertIndex].m_position  = Vec3(0.f, 0.f, 0.f);
        m_localVerts[secondVertIndex].m_position = Vec3(secondVertOfs.x, secondVertOfs.y, 0.f);
        m_localVerts[thirdVertIndex].m_position  = Vec3(thirdVertOfs.x, thirdVertOfs.y, 0.f);
    }

    // Set colors
    for (int vertIndex = 0; vertIndex < NUM_ASTEROID_VERTS; ++vertIndex)
    {
        m_localVerts[vertIndex].m_color = m_color;
    }
}

void Asteroid::OnColliedEnter(Entity* other)
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
    //g_theAudio->StartSound(SOUND::ASTEROIDS_DAMAGED);
    ParticleHandler::getInstance()->SpawnNewParticleCluster(pp);
}
