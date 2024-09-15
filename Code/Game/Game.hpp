#pragma once
#include "Asteroid.h"
#include "Bullet.h"
#include "GameCommon.hpp"
#include "PlayerShip.hpp"


/*
*   a. (1) Gameplay overseer, bookkeeper, and referee – owns all gameplay-related concepts
    b. (1) Handles all entity-vs-entity interactions (e.g. physics, damage)
    c. (1) Handles all high-level game mechanics (e.g. levels/waves, spawning)
    d. (1) Only one instance, which is owned (created, managed, destroyed) by g_theApp
    e. (2) Owns all gameplay Entity instances (e.g. m_playerShip, m_bullets, m_asteroids).
    The ship should be a single object (by pointer); bullets and asteroids are each stored in
    a separate fixed-size c-style list (array) of pointers, as such:
*/
class Game
{
public:
    Game();
    ~Game();
    void Render();
    void Update(float deltaTime);

    void SpawnNewBullet(Vec2 const & position, float orientationDegrees);

private:
    void RenderEntities();
    void HandleEntityCollisions();

public:
    PlayerShip*     m_PlayerShip = nullptr; // Just one player ship (for now...)
    Asteroid*       m_asteroid[MAX_ASTEROIDS] = {}; // Fixed number of asteroid “slots” nullptr if unused.
    Bullet*         m_bullets[MAX_BULLETS] = {}; // The “= {};” syntax initializes the array to zeros.
};
