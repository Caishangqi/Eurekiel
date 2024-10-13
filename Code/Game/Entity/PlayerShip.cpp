#include "PlayerShip.hpp"

#include "Game/Game.hpp"
#include "Game/Entity/Cube.hpp"


PlayerShip::PlayerShip(Game* gameInstance, const Vec2& startPosition, float orientationDegree): Entity(
    gameInstance, startPosition, orientationDegree)
{
    m_color              = Rgba8(102, 153, 204);
    m_physicsRadius      = PLAYER_SHIP_PHYSICS_RADIUS;
    m_cosmeticRadius     = PLAYER_SHIP_COSMETIC_RADIUS;
    m_orientationDegrees = orientationDegree;
    m_health             = 1;
    PlayerShip::InitializeLocalVerts();
}

PlayerShip::~PlayerShip()
{
}

void PlayerShip::Update(float deltaSeconds)
{
    m_particleTimer += deltaSeconds;

    if (m_isDead)
    {
        return;
    }

    Entity::Update(deltaSeconds);
    UpdateFromInputSystem(deltaSeconds);
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
        Vec2 fwdNormal    = Vec2::MakeFromPolarDegrees(m_orientationDegrees);
        Vec2 acceleration = fwdNormal * PLAYER_SHIP_ACCELERATION * m_thrustRate;
        m_velocity += acceleration * deltaSeconds;
    }

    // Update particle
    if (m_isThrusting)
    {
        if (m_particleTimer >= .1f) // 如果计时器达到1秒钟
        {
            FParticleProperty pp;
            pp.fadeOpacity        = true;
            pp.numDebris          = static_cast<int>(10 * m_thrustRate);
            pp.averageVelocity    = Vec2::MakeFromPolarDegrees(m_orientationDegrees) * -10;
            pp.maxScatterSpeed    = 10.f;
            pp.color              = Rgba8(224, 218, 136);
            pp.position           = m_position + Vec2::MakeFromPolarDegrees(m_orientationDegrees) * -2;
            pp.minAngularVelocity = 0.f;
            pp.maxAngularVelocity = 0.f;

            pp.minOpacity = 0.1f;
            pp.maxOpacity = 0.4f;

            pp.minLifeTime = .2f;
            pp.maxLifeTime = .4f;

            FParticleProperty pp_flame;
            pp_flame.fadeOpacity        = true;
            pp_flame.numDebris          = static_cast<int>(10 * m_thrustRate);
            pp_flame.averageVelocity    = Vec2::MakeFromPolarDegrees(m_orientationDegrees) * -10;
            pp_flame.maxScatterSpeed    = 10.f;
            pp_flame.color              = Rgba8(230, 167, 60);
            pp_flame.position           = m_position + Vec2::MakeFromPolarDegrees(m_orientationDegrees) * -2;
            pp_flame.minAngularVelocity = 0.f;
            pp_flame.maxAngularVelocity = 0.f;

            pp_flame.minOpacity = 0.1f;
            pp_flame.maxOpacity = 0.4f;

            pp_flame.minLifeTime = .4f;
            pp_flame.maxLifeTime = .8f;


            ParticleHandler::getInstance()->SpawnNewParticleCluster(pp);
            ParticleHandler::getInstance()->SpawnNewParticleCluster(pp_flame);

            m_particleTimer = 0.f;
        }
    }

    // Move the ship (OLD)
    // m_position += m_velocity * deltaSeconds;

    // Move the ship
    m_position += m_velocity * PLAYER_SHIP_SPEED * deltaSeconds;

    if (m_health <= 0)
    {
        m_isDead = true;
        Die();
    }


    BounceOffWalls();
}

void PlayerShip::Render() const
{
    if (!m_isDead)
    {
        Vertex_PCU tempWorldVerts[NUM_SHIP_VERTS];
        for (int vertIndex = 0; vertIndex < NUM_SHIP_VERTS; vertIndex++)
        {
            tempWorldVerts[vertIndex] = m_localVerts[vertIndex];
        }
        TransformVertexArrayXY3D(NUM_SHIP_VERTS, tempWorldVerts, 1.f, m_orientationDegrees, m_position);
        g_renderer->DrawVertexArray(NUM_SHIP_VERTS, tempWorldVerts);

        // Tail Flame
        Vertex_PCU spaceShipTailFlame[12];
        float      randomRate = g_rng->RollRandomFloatInRange(0.4f, 0.8f);
        for (int vertIndex = 0; vertIndex < 12; vertIndex++)
        {
            spaceShipTailFlame[vertIndex]           = ICON::SPACESHIP_TAIL_FLAME[vertIndex];
            spaceShipTailFlame[vertIndex].m_color.a = static_cast<unsigned char>(spaceShipTailFlame[vertIndex].m_color.a
                * m_thrustRate * randomRate);
        }
        TransformVertexArrayXY3D(12, spaceShipTailFlame, 1.f, m_orientationDegrees, m_position);
        g_renderer->DrawVertexArray(12, spaceShipTailFlame);
    }
    DebugRender();
}

void PlayerShip::Die()
{
    Entity::Die();
    m_game->OnPlayerShipDeathEvent(this);
}


void PlayerShip::InitializeLocalVerts()
{
    // Nose cone
    m_localVerts[0].m_position = ICON::SPACESHIP[0].m_position;
    m_localVerts[1].m_position = ICON::SPACESHIP[1].m_position;
    m_localVerts[2].m_position = ICON::SPACESHIP[2].m_position;


    // Left Wing
    m_localVerts[3].m_position = Vec3(2.f, 1.f, 0.f);
    m_localVerts[4].m_position = Vec3(0.f, 2.f, 0.f);
    m_localVerts[5].m_position = Vec3(-2.f, 1.f, 0.f);

    // Right Wing
    m_localVerts[6].m_position = Vec3(2.f, -1.f, 0.f);
    m_localVerts[7].m_position = Vec3(-2.f, -1.f, 0.f);
    m_localVerts[8].m_position = Vec3(0.f, -2.f, 0.f);

    // Body
    m_localVerts[9].m_position  = Vec3(0.f, 1.f, 0.f);
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
    const XboxController& controller = g_theInput->GetController(0);


    auto movementDirection = Vec2(0.f, 0.f);

    // 根据键盘输入更新移动方向
    if (g_theInput->IsKeyDown('W'))
    {
        movementDirection += Vec2(0.f, 1.f); // 向上移动
    }
    if (g_theInput->IsKeyDown('S'))
    {
        movementDirection += Vec2(0.f, -1.f); // 向下移动
    }
    if (g_theInput->IsKeyDown('A'))
    {
        movementDirection += Vec2(-1.f, 0.f); // 向左移动
    }
    if (g_theInput->IsKeyDown('D'))
    {
        movementDirection += Vec2(1.f, 0.f); // 向右移动
    }

    // 正常化方向向量，以便斜向移动时保持恒定速度
    if (movementDirection.GetLength() > 0.f)
    {
        movementDirection.Normalize();
        m_velocity = movementDirection;
    }
    else if (!g_theInput->IsKeyDown('W') || !g_theInput->IsKeyDown('S') || !g_theInput->IsKeyDown('A') || !g_theInput->
        IsKeyDown('D'))
    {
        m_velocity *= 0.9f;
        if (m_velocity.GetLength() < 0.01f)
        {
            m_velocity = Vec2(0.f, 0.f);
        }
    }
    auto button          = Vec2(0, 0);
    auto topRight        = Vec2(200, 100);
    Vec2 mousePosition   = g_theInput->GetMousePositionOnWorld(button, topRight);
    m_orientationDegrees = (mousePosition - m_position).GetOrientationDegrees();

    //printf("Magnitude: %f\n", controller.GetLeftStick().GetMagnitude());
    /*m_isTurningLeft  = g_theInput->IsKeyDown('A');
    m_isTurningRight = g_theInput->IsKeyDown('D');

    // A combination between 2 controller
    if (g_theInput->IsKeyDown('W') || controller.GetLeftStick().GetMagnitude() > 0.f)
    {
        if (g_theInput->IsKeyDown('W'))
        {
            m_isThrusting = true;
            m_thrustRate  = 1;
        }
        if (controller.GetLeftStick().GetMagnitude() > 0.f)
        {
            m_isThrusting = true;
            m_thrustRate  = controller.GetLeftStick().GetMagnitude();
        }
    }
    else
    {
        m_isThrusting = false;
        m_thrustRate  = 0;
    }*/


    if (g_theInput->WasMouseButtonJustPressed(0))
    {
        m_game->SpawnNewBullet(m_position + m_velocity.GetNormalized(), m_orientationDegrees);
    }

    if (g_theInput->WasKeyJustPressed(' '))
    {
        m_game->SpawnNewBullet(m_position + m_velocity.GetNormalized(), m_orientationDegrees);
    }
}

void PlayerShip::UpdateFromController(float& deltaSeconds)
{
    const XboxController& controller = g_theInput->GetController(0);
    if (controller.GetLeftStick().GetMagnitude() > 0.f)
    {
        m_orientationDegrees = controller.GetLeftStick().GetPosition().GetOrientationDegrees();
    }

    if (controller.WasButtonJustPressed(XBOX_BUTTON_A))
    {
        m_game->SpawnNewBullet(m_position + m_velocity.GetNormalized(), m_orientationDegrees);
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
    if (m_isDead)
    {
        m_isDead             = false;
        m_position           = Vec2(WORLD_CENTER_X, WORLD_CENTER_Y);
        m_velocity           = Vec2(0.f, 0.f);
        m_orientationDegrees = 0.f;
        m_health             = 1;
        m_thrustRate         = 0.f;
        m_game->remainTry--;
        if (m_game->remainTry > 0)
        {
            m_game->OnPlayerShipRespawnEvent(this, m_game->remainTry);
        }
    }
}

void PlayerShip::OnColliedEnter(Entity* other)
{
    if (typeid(*other).name() == typeid(Cube).name())
    {
        m_velocity = -m_velocity;
        return;
    }

    Entity::OnColliedEnter(other);
    m_health--;
    FParticleProperty pp;
    pp.fadeOpacity        = true;
    pp.numDebris          = 80;
    pp.averageVelocity    = m_velocity + other->m_velocity;
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

void PlayerShip::UpdateFromInputSystem(float& deltaSeconds)
{
    UpdateFromController(deltaSeconds);
    UpdateFromKeyBoard(deltaSeconds);
}
