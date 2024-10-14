#include "Cube.hpp"

#include <vcruntime_typeinfo.h>

#include "PlayerShip.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/Game.hpp"


#include "Game/GameCommon.hpp"
#include "Game/Grid/Grid.hpp"
#include "Game/Particle/FParticleProperty.hpp"
#include "Game/Particle/ParticleHandler.hpp"
#include "Game/Resource/SoundRes.hpp"
#include "Game/Widget/Data/IconRes.hpp"
#include "Tetromino/BaseTetromino.hpp"

Cube::Cube(Game* owner, const Vec2& startPosition, float orientationDegree)
    : Entity(owner, startPosition, orientationDegree)
{
    m_velocity = Vec2(-20, 0);

    m_aabb = new AABB2();
    m_aabb->SetCenter(startPosition);
    m_aabb->SetDimensions(Vec2(5, 5));

    m_physicsRadius = 2.5f;

    m_health = 1;
}

Cube::Cube(Game* owner, Grid* grid, const Vec2& startPosition, float orientationDegree): Entity(
    owner, startPosition, orientationDegree)
{
    m_velocity   = Vec2(-40, 0);
    m_parentGrid = grid;

    m_aabb = new AABB2();
    m_aabb->SetCenter(startPosition);
    m_aabb->SetDimensions(Vec2(5, 5));

    m_health = 1;
}

Cube::Cube(Game* owner, Grid* grid, const IntVec2& startPosition, float orientationDegree)
{
}


Cube::Cube()
{
}

Cube::~Cube()
{
    delete m_aabb;
    m_aabb = nullptr;

    m_game       = nullptr;
    m_parentGrid = nullptr;
}

void Cube::Update(float deltaSeconds)
{
    if (m_IsStatic)
    {
        return;
    }

    m_aabb->SetCenter(m_position);
    m_position += m_velocity * deltaSeconds;

    // (39, 19)
    if (m_position.x <= m_parentGrid->GetHorizontalBaseLine(m_gridPos.y))
    {
        //m_position.x = GRID_BASE_LINE_X;
        SetCubeStatic();
        m_parentGrid->OnCubeTouchBaseLineEvent(this);
    }


    // Life management
    if (m_health <= 0)
    {
        m_isDead = true;
        Die();
    }


    if (m_isDead)
        m_isGarbage = true;
}

void Cube::Render() const
{
    if (!m_isDead)
    {
        // Tail Flame
        Vertex_PCU Cube[6];
        for (int vertIndex = 0; vertIndex < 6; vertIndex++)
        {
            Cube[vertIndex] = ICON::CUBE[vertIndex];
        }
        TransformVertexArrayXY3D(6, Cube, 1.f, m_orientationDegrees, m_position);
        g_renderer->DrawVertexArray(6, Cube);
    }
    DebugRender();
}

void Cube::InitializeLocalVerts()
{
}

void Cube::DebugRender() const
{
    Entity::DebugRender();
}

void Cube::Die()
{
    Entity::Die();
    if (GetParentTetromino())
    {
        if (!m_IsStatic)
        {
            // TODO: check validation
            GetParentTetromino()->MarkCubeAsGarbage(this);
            GetParentTetromino()->RemoveCubePointerInTetromino(this);
            //GetParentTetromino()->m_cubes[m_TetrominoPosition.x][m_TetrominoPosition.y] = nullptr;
        }
    }
    m_parentGrid->OnCubeDieEvent(this);

    m_game->OnPointGainEvent(1);
}

void Cube::OnColliedEnter(Entity* other)
{
    if (typeid(*other).name() == typeid(PlayerShip).name())
    {
        return;
    }

    if (m_IsStatic)
    {
        return;
    }

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
    g_theAudio->StartSound(SOUND::ENEMY_DAMAGED);

    ParticleHandler::getInstance()->SpawnNewParticleCluster(pp);
}

bool Cube::IsAlive() const
{
    return Entity::IsAlive();
}

bool Cube::SetCubeStatic()
{
    if (m_IsStatic)
    {
        m_velocity = Vec2(0, 0);
        return false;
    }
    m_IsStatic = true;
    m_velocity = Vec2(0, 0);
    return true;
}

void Cube::UpdateGridPosition()
{
    m_gridPos = Grid::GetGridPositionFromPosition(m_position);
}

void Cube::CorrectWorldPosition()
{
    //Correct position
    m_position = m_parentGrid->GetPositionFromGrid(m_gridPos);
    m_aabb->SetCenter(m_position);
}

void Cube::SetParentTetromino(BaseTetromino* parentTetromino)
{
    m_parentTetromino = parentTetromino;
}

BaseTetromino* Cube::GetParentTetromino() const
{
    return m_parentTetromino;
}
