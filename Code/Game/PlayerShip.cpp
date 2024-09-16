#include "PlayerShip.hpp"

#include "App.hpp"
#include "GameCommon.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"

PlayerShip::PlayerShip(Game* gameInstance, const Vec2& startPosition, float orientationDegree): Entity(
    gameInstance, startPosition, orientationDegree)
{
    m_physicsRadius = PLAYER_SHIP_PHYSICS_RADIUS;
    m_cosmeticRadius = PLAYER_SHIP_COSMETIC_RADIUS;
    m_orientationDegrees = orientationDegree;
    m_health = 1;
    PlayerShip::InitializeLocalVerts();
    /*m_vertices = static_cast<Vertex_PCU*>(
        malloc(sizeof(*m_vertices) * 15));*/
}

PlayerShip::~PlayerShip()
{
}

void PlayerShip::Update(float deltaSeconds)
{
    if (m_isDead)
    {
        return;
    }

    Entity::Update(deltaSeconds);

    UpdateFromKeyBoard(deltaSeconds);
    if (m_isTurningLeft)
    {
        m_orientationDegrees += deltaSeconds * PLAYER_SHIP_TURN_SPEED;
    }
    if (m_isTurningRight)
    {
        m_orientationDegrees -= deltaSeconds * PLAYER_SHIP_TURN_SPEED;
    }
    if (m_isThrusting)
    {
        Vec2 fwdNormal = Vec2::MakeFromPolarDegrees(m_orientationDegrees);
        Vec2 acceleration = fwdNormal * PLAYER_SHIP_ACCELERATION;
        m_velocity += acceleration * deltaSeconds;
    }
    // Move the ship
    m_position += m_velocity * deltaSeconds;

    if (m_health <= 0)
        m_isDead = true;

    BounceOffWalls();
}

void PlayerShip::Render() const
{
    if (m_isDead)
    {
        return;
    }
    Vertex_PCU tempWorldVerts[NUM_SHIP_VERTS];
    for (int vertIndex = 0; vertIndex < NUM_SHIP_VERTS; vertIndex++)
    {
        tempWorldVerts[vertIndex] = m_localVerts[vertIndex];
    }
    TransformVertexArrayXY3D(NUM_SHIP_VERTS, tempWorldVerts, 1.f, m_orientationDegrees, m_position);
    g_renderer->DrawVertexArray(NUM_SHIP_VERTS, tempWorldVerts);
}


void PlayerShip::InitializeLocalVerts()
{
    // Nose cone
    m_localVerts[0].m_position = Vec3(1.f, 0.f, 0.f);
    m_localVerts[1].m_position = Vec3(0.f, 1.f, 0.f);
    m_localVerts[2].m_position = Vec3(0.f, -1.f, 0.f);

    // Left Wing
    m_localVerts[3].m_position = Vec3(2.f, 1.f, 0.f);
    m_localVerts[4].m_position = Vec3(0.f, 2.f, 0.f);
    m_localVerts[5].m_position = Vec3(-2.f, 1.f, 0.f);

    // Right Wing
    m_localVerts[6].m_position = Vec3(2.f, -1.f, 0.f);
    m_localVerts[7].m_position = Vec3(-2.f, -1.f, 0.f);
    m_localVerts[8].m_position = Vec3(0.f, -2.f, 0.f);

    // Body
    m_localVerts[9].m_position = Vec3(0.f, 1.f, 0.f);
    m_localVerts[10].m_position = Vec3(-2.f, -1.f, 0.f);
    m_localVerts[11].m_position = Vec3(0.f, -1.f, 0.f);

    // Body
    m_localVerts[12].m_position = Vec3(0.f, 1.f, 0.f);
    m_localVerts[13].m_position = Vec3(-2.f, 1.f, 0.f);
    m_localVerts[14].m_position = Vec3(-2.f, -1.f, 0.f);

    for (int vertIndex = 0; vertIndex < NUM_SHIP_VERTS; vertIndex++)
    {
        m_localVerts[vertIndex].m_color = Rgba8(102, 153, 204);
    }
}

void PlayerShip::UpdateFromKeyBoard(float& deltaSeconds)
{
    m_isTurningLeft = g_theApp->IsKeyDown('S');
    m_isTurningRight = g_theApp->IsKeyDown('F');
    m_isThrusting = g_theApp->IsKeyDown('E');

    if (g_theApp->WasKeyJustPressed(' '))
    {
        m_game->SpawnNewBullet(Vec2(m_position.x, m_position.y), m_orientationDegrees);
    }
}

void PlayerShip::BounceOffWalls()
{
    if (m_position.x < m_physicsRadius)
    {
        // We will always see the ship stick on bound no matter how fast is ship
        m_position.x = m_physicsRadius;
        m_velocity.x = -m_velocity.x;
    }

    if (m_position.x > WORLD_SIZE_X - m_physicsRadius) //e.g. 197 if world is 200 wide and radius is 3
    {
        m_position.x = WORLD_SIZE_X - m_physicsRadius;
        m_velocity.x = -m_velocity.x;
    }
    if (m_position.y < m_physicsRadius)
    {
        // We will always see the ship stick on bound no matter how fast is ship
        m_position.y = m_physicsRadius;
        m_velocity.y = -m_velocity.y;
    }

    if (m_position.y > WORLD_SIZE_Y - m_physicsRadius) //e.g. 197 if world is 200 wide and radius is 3
    {
        m_position.y = WORLD_SIZE_Y - m_physicsRadius;
        m_velocity.y = -m_velocity.y;
    }
}

void PlayerShip::Respawn()
{
    m_isDead = false;
    m_position = Vec2(WORLD_CENTER_X, WORLD_CENTER_Y);
    m_velocity = Vec2(0.f, 0.f);
    m_orientationDegrees = 90.f;
    m_health = 1;
}
