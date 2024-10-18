#pragma once
#include "GameCommon.hpp"
#include "Enum/EGameState.h"

struct GameChangeStateEvent;
struct PointGainEvent;
class Asteroid;
class Entity;
class Bullet;
class PlayerShip;
class Grid;
class Cube;
class Camera;
struct FTimerHandle;
class WidgetHandler;
class ParticleHandler;
class LevelHandler;
enum EEntity : int;
class Wasp;
class Beetle;
/*
 *  a. (1) Gameplay overseer, bookkeeper, and referee – owns all gameplay-related concepts
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
    void Render() const;

    void Update(float deltaTime);

    void SpawnNewBullet(const Vec2& position, float orientationDegrees);
    void SpawnNewAsteroids();
    void HandleKeyBoardEvent(float deltaTime);
    void HandleMouseEvent(float deltaTime);
    void RegisterDefaultObjects();
    // Game play logic
    void StartGame();

    // Update
    void UpdateTimerHandle(float deltaSeconds);
    void UpdateBullet(float deltaTime);
    void UpdateAsteroid(float deltaTime);
    void UpdateBeetle(float deltaTime);
    void UpdateWasp(float deltaTime);
    void UpdateCube(float deltaTime);

    // Camera
    void UpdateCameras(float deltaTime);

private:
    void SpawnDefaultAsteroids();
    Vec2 getRandomPositionOffscreen(Entity* entity) const;

    void RenderEntities() const;
    void HandleEntityCollisions();
    void GarbageCollection();

public:
    /**
     * Spawn specific entity type on the game world
     * @param entityType The specific entity type
     */
    void Spawn(EEntity entityType);

    /**
     * Spawn specific entity type on the game world
     * @param entityType The specific entity type
     */
    void Spawn(EEntity entityType, int amount);


    // Simple event 
    void OnEntityDieEvent(Entity* entity);
    void OnPlayerShipRespawnEvent(PlayerShip* playerShip, int remainTry);
    void OnPlayerShipDeathEvent(PlayerShip* playerShip);
    void OnMainMenuDisplayEvent();
    void OnPointGainEvent(int gainedScore);
    // Event handle
    void OnPointGainEvent(std::shared_ptr<PointGainEvent> event);
    void OnPointGainEvent_(PointGainEvent * event);
    void OnGameChangeStateEvent(std::shared_ptr<GameChangeStateEvent> event);

    // nullptr equal to 0
    PlayerShip* m_PlayerShip                           = nullptr; // Just one player ship (for now...)
    Asteroid*   m_asteroids[MAX_ASTEROIDS]             = {}; // Fixed number of asteroid “slots” nullptr if unused.
    Bullet*     m_bullets[MAX_BULLETS]                 = {}; // The “= {};” syntax initializes the array to zeros.
    Beetle*     m_entities_beetle[MAX_ENTITY_PER_TYPE] = {};
    Wasp*       m_entity_wasp[MAX_ENTITY_PER_TYPE]     = {};
    Cube*       m_cube[1024]                           = {};

    LevelHandler* m_levelHandler = nullptr; // LevelHandler

    WidgetHandler* m_widgetHandler = nullptr; // Widget Handler

    int remainTry = NUM_MAX_TRY;

    bool IsInMainMenu = true;
    bool IsGameStart  = false;

    // Camera
    Camera* m_worldCamera  = nullptr;
    Camera* m_screenCamera = nullptr;

    // Grid system
    Grid* m_grid = nullptr;

    int Score = 0;

    // Game State
    EGameState m_gameState = EGameState::MENU;

private:
    FTimerHandle* m_timerHandles[128] = {};

public:
    FTimerHandle* SetTimer(float timer, void (*callback)());
};
