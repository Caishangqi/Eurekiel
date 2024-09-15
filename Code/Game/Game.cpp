#include "Game.hpp"

Game::Game()
{
    // Spawn the ship at world center
    m_PlayerShip = new PlayerShip(this, Vec2(WORLD_CENTER_X, WORLD_CENTER_Y
                                  ), 30.f);
}

Game::~Game()
{
    delete m_PlayerShip;
    m_PlayerShip = nullptr;

    delete m_bullets;
}


void Game::Render()
{
    RenderEntities();
}

void Game::Update(float deltaTime)
{
    m_PlayerShip->Update(deltaTime);

    for (int bulletIndex = 0; bulletIndex < MAX_BULLETS; bulletIndex++)
    {
        if (m_bullets[bulletIndex] != nullptr)
        {
            m_bullets[bulletIndex]->Update(deltaTime);
        }
    }

    HandleEntityCollisions();
}

void Game::SpawnNewBullet(Vec2 const& position, float orientationDegrees)
{
    Bullet* bullet = new Bullet(this, position, orientationDegrees);
    for (int i = 0; i < MAX_BULLETS; i++)
    {
        if (m_bullets[i] == nullptr)
        {
            m_bullets[i] = bullet;
            break;
        }
    }
}

void Game::RenderEntities()
{
    m_PlayerShip->Render();
    for (int bulletIndex = 0; bulletIndex < MAX_BULLETS; bulletIndex++)
    {
        if (m_bullets[bulletIndex] != nullptr)
        {
            m_bullets[bulletIndex]->Render();
        }
    }
}

void Game::HandleEntityCollisions()
{
}
