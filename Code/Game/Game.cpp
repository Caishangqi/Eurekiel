#include "Game.hpp"

#include "App.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

Game::Game()
{
    // Spawn the ship at world center
    m_PlayerShip = new PlayerShip(this, Vec2(WORLD_CENTER_X, WORLD_CENTER_Y
                                  ), 30.f);
    SpawnDefaultAsteroids();
}

Game::~Game()
{
    // TODO: Why can not delete individual elements
    /*for (auto& m_bullet : m_bullets)
    {
        if (m_bullet != nullptr)
        {
            delete m_bullet;
            m_bullet = nullptr;
        }
    }*/

    //delete m_bullets;


    delete m_PlayerShip;
    m_PlayerShip = nullptr;
}


void Game::Render()
{
    RenderEntities();
}

void Game::Update(float deltaTime)
{
    m_PlayerShip->Update(deltaTime);

    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet != nullptr)
        {
            m_bullet->Update(deltaTime);
        }
    }

    for (Asteroid*& m_asteroid : m_asteroid)
    {
        if (m_asteroid != nullptr)
            m_asteroid->Update(deltaTime);
    }

    HandleKeyBoardEvent(deltaTime);
    HandleEntityCollisions();
    GarbageCollection();
}

void Game::SpawnNewBullet(Vec2 const& position, float orientationDegrees)
{
    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet == nullptr)
        {
            m_bullet = new Bullet(this, position, orientationDegrees);
            return;
        }
    }
}

void Game::SpawnDefaultAsteroids()
{
    for (int numberAsteroid = 0; numberAsteroid < NUM_STARTING_ASTEROIDS; ++numberAsteroid)
    {
        // Generate random position in world 
        float worldPositionX = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_X);
        float worldPositionY = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_Y);
        float randomOrientation = g_rng->RollRandomFloatInRange(0, 360);
        m_asteroid[numberAsteroid] = new Asteroid(this, Vec2(worldPositionX, worldPositionY), randomOrientation);
    }
}

void Game::SpawnNewAsteroids()
{
    for (int numberAsteroid = 0; numberAsteroid < MAX_ASTEROIDS; ++numberAsteroid)
    {
        if (m_asteroid[numberAsteroid] == nullptr)
        {
            // Generate random position in world 
            float worldPositionX = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_X);
            float worldPositionY = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_Y);
            float randomOrientation = g_rng->RollRandomFloatInRange(0, 360);
            m_asteroid[numberAsteroid] = new Asteroid(this, Vec2(worldPositionX, worldPositionY), randomOrientation);
            return;
        }
    }
    // Which means reach the max of spawning
}

void Game::HandleKeyBoardEvent(float deltaTime)
{
    if (g_theApp->WasKeyJustPressed('N'))
        m_PlayerShip->Respawn();
}

void Game::RenderEntities()
{
    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet != nullptr)
        {
            m_bullet->Render();
        }
    }

    for (Asteroid*& asteroid : m_asteroid)
    {
        if (asteroid != nullptr)
        {
            asteroid->Render();
        }
    }

    m_PlayerShip->Render();
}

void Game::HandleEntityCollisions()
{
    // Handle Bullet Asteroids Collision
    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet != nullptr && m_bullet->IsAlive())
        {
            for (Asteroid*& asteroid : m_asteroid)
            {
                if (asteroid != nullptr && asteroid->IsAlive())
                {
                    if (DoDiscsOverlap(m_bullet->m_position, m_bullet->m_physicsRadius, asteroid->m_position,
                                       asteroid->m_physicsRadius))
                    {
                        m_bullet->m_health -= 1;
                        asteroid->m_health -= 1;
                    }
                }
            }
        }
    }

    for (Asteroid*& asteroid : m_asteroid)
    {
        if (asteroid != nullptr && asteroid->IsAlive())
        {
            if (DoDiscsOverlap(asteroid->m_position, asteroid->m_physicsRadius, m_PlayerShip->m_position,
                               m_PlayerShip->m_physicsRadius))
            {
                m_PlayerShip->m_health -= 1;
            }
        }
    }
}

void Game::GarbageCollection()
{
    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet != nullptr && m_bullet->IsGarbage())
        {
            delete m_bullet;
            m_bullet = nullptr;
        }
    }

    for (Asteroid*& asteroid : m_asteroid)
    {
        if (asteroid != nullptr && asteroid->IsGarbage())
        {
            delete asteroid;
            asteroid = nullptr;
        }
    }
}
