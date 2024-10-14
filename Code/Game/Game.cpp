#include "Game.hpp"
#include "GameCommon.hpp"
#include "Entity/Asteroid.hpp"
#include "Entity/Beetle.hpp"
#include "Entity/Bullet.hpp"
#include "Entity/Cube.hpp"
#include "Entity/PlayerShip.hpp"
#include "Entity/Wasp.hpp"
#include "Enum/EEntity.h"
#include "Grid/Grid.hpp"
#include "Level/LevelHandler.hpp"
#include "Time/FTimerHandle.hpp"
#include "Widget/WidgetHandler.hpp"
#include "Widget/Widgets/WidgetMainMenu.hpp"
#include "Widget/Widgets/WidgetPlayerHealth.hpp"

Game::Game()
{
    RegisterDefaultObjects();
    // Spawn the ship at world center

    m_levelHandler  = new LevelHandler(this);
    m_widgetHandler = new WidgetHandler(this);

    // Cameras
    m_screenCamera = new Camera();
    m_worldCamera  = new Camera();

    // Grid
    m_grid = new Grid(this);
}

Game::~Game()
{
    delete m_PlayerShip;
    m_PlayerShip = nullptr;

    delete m_levelHandler;
    m_levelHandler = nullptr;

    delete m_widgetHandler;
    m_widgetHandler = nullptr;

    delete m_grid;
    m_grid = nullptr;
    ParticleHandler::getInstance()->CleanParticle();
}


void Game::Render() const
{
    // Fist render world camera
    g_renderer->BeingCamera(*m_worldCamera);
    if (IsGameStart)
    {
        RenderEntities();
    }
    ParticleHandler::getInstance()->Render();
    g_renderer->EndCamera(*m_worldCamera);
    //======================================================================= End of World Render =======================================================================

    // Second render screen camera
    g_renderer->BeingCamera(*m_screenCamera);
    m_widgetHandler->Render();
    g_renderer->EndCamera(*m_screenCamera);
    //======================================================================= End of Screen Render =======================================================================
}

void Game::UpdateBullet(float deltaTime)
{
    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet != nullptr)
        {
            m_bullet->Update(deltaTime);
        }
    }
}

void Game::UpdateAsteroid(float deltaTime)
{
    for (Asteroid*& m_asteroid : m_asteroids)
    {
        if (m_asteroid != nullptr)
            m_asteroid->Update(deltaTime);
    }
}

void Game::UpdateBeetle(float deltaTime)
{
    for (Beetle* element : m_entities_beetle)
    {
        if (element != nullptr)
            element->Update(deltaTime);
    }
}

void Game::UpdateWasp(float deltaTime)
{
    for (Wasp* element : m_entity_wasp)
    {
        if (element != nullptr)
            element->Update(deltaTime);
    }
}

void Game::UpdateCube(float deltaTime)
{
    for (Cube* element : m_cube)
    {
        if (element != nullptr)
            element->Update(deltaTime);
    }
}

void Game::UpdateCameras(float deltaTime)
{
    m_worldCamera->SetOrthoView(Vec2(0, 0), Vec2(200, 100));
    m_worldCamera->Update(deltaTime);
    m_screenCamera->SetOrthoView(Vec2(0, 0), Vec2(1600, 800));
    m_screenCamera->Update(deltaTime);
}


void Game::Update(float deltaTime)
{
    UpdateCameras(deltaTime);

    m_widgetHandler->Update(deltaTime);
    ParticleHandler::getInstance()->Update(deltaTime);
    if (m_PlayerShip)
        m_PlayerShip->Update(deltaTime);

    UpdateTimerHandle(deltaTime);
    if (IsGameStart)
    {
        UpdateBullet(deltaTime);

        UpdateAsteroid(deltaTime);

        UpdateBeetle(deltaTime);

        UpdateWasp(deltaTime);

        UpdateCube(deltaTime);

        m_grid->Update(deltaTime);

        HandleEntityCollisions();
    }


    m_levelHandler->Update(deltaTime);

    HandleKeyBoardEvent(deltaTime);

    GarbageCollection();
}

void Game::SpawnNewBullet(const Vec2& position, float orientationDegrees)
{
    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet == nullptr)
        {
            // TODO: 为什么这部分移动到PlayerShip会有编译错误
            g_theAudio->StartSound(SOUND::PLAYER_SHOOTS_BULLET);
            m_bullet = new Bullet(this, position, orientationDegrees);
            return;
        }
    }
    ERROR_RECOVERABLE("MAX AMOUNT OF ENTITY REACH")
}

void Game::SpawnDefaultAsteroids()
{
    for (int numberAsteroid = 0; numberAsteroid < NUM_STARTING_ASTEROIDS; ++numberAsteroid)
    {
        // Generate random position in world 
        float worldPositionX        = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_X);
        float worldPositionY        = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_Y);
        float randomOrientation     = g_rng->RollRandomFloatInRange(0, 360);
        auto  AddedAsteroid         = new Asteroid(this, Vec2(worldPositionX, worldPositionY), randomOrientation);
        Vec2  offScreenPos          = getRandomPositionOffscreen(AddedAsteroid);
        AddedAsteroid->m_position   = offScreenPos;
        m_asteroids[numberAsteroid] = AddedAsteroid;
    }
}

Vec2 Game::getRandomPositionOffscreen(Entity* entity) const
{
    float cosmeticRadius = entity->m_cosmeticRadius;

    int edge = g_rng->RollRandomIntInRange(0, 3); // 随机选择一条边 0:下边, 1:上边, 2:左边, 3:右边

    Vec2 point;

    switch (edge)
    {
    case 0: // 下边 (0, 0) 到 (200, 0)
        point.x = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_X + cosmeticRadius);
        point.y = 0;
        break;
    case 1: // 上边 (0, 100) 到 (200, 100)
        point.x = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_X + cosmeticRadius);
        point.y = WORLD_SIZE_Y;
        break;
    case 2: // 左边 (0, 0) 到 (0, 100)
        point.x = 0;
        point.y = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_Y + cosmeticRadius);
        break;
    case 3: // 右边 (200, 0) 到 (200, 100)
        point.x = WORLD_SIZE_X;
        point.y = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_Y + cosmeticRadius);
        break;
    default:
        printf("(!) Invalid edge value! It should not happened\n");
    }

    return point;
}

void Game::SpawnNewAsteroids()
{
    for (int numberAsteroid = 0; numberAsteroid < MAX_ASTEROIDS; ++numberAsteroid)
    {
        if (m_asteroids[numberAsteroid] == nullptr)
        {
            // Generate random position in world 
            float worldPositionX        = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_X);
            float worldPositionY        = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_Y);
            float randomOrientation     = g_rng->RollRandomFloatInRange(0, 360);
            m_asteroids[numberAsteroid] = new Asteroid(this, Vec2(worldPositionX, worldPositionY), randomOrientation);
            return;
        }
    }
    // Which means reach the max of spawning
    ERROR_RECOVERABLE("MAX AMOUNT OF ENTITY REACH")
}

void Game::HandleKeyBoardEvent(float deltaTime)
{
    const XboxController& controller = g_theInput->GetController(0);
    if (IsInMainMenu)
    {
        bool spaceBarPressed = g_theInput->WasKeyJustPressed(32) || controller.WasButtonJustPressed(XBOX_BUTTON_A);
        bool NKeyPressed     = g_theInput->WasKeyJustPressed('N') || controller.WasButtonJustPressed(XBOX_BUTTON_START);
        if (spaceBarPressed || NKeyPressed)
        {
            StartGame();
        }
    }

    if (g_theInput->WasKeyJustPressed('X'))
    {
        m_levelHandler->GenerateRandomTetromino();
    }


    // Return to Main menu
    if (g_theInput->WasKeyJustPressed(27))
    {
        ReturnToMainMenu();
    }

    if (controller.WasButtonJustPressed(XBOX_BUTTON_B))
    {
        ReturnToMainMenu();
    }


    if (g_theInput->WasKeyJustPressed('N') || controller.WasButtonJustPressed(XBOX_BUTTON_START))
    {
        if (m_PlayerShip != nullptr && !m_PlayerShip->IsAlive())
        {
            if (remainTry > 1)
            {
                m_PlayerShip->Respawn();
            }
        }
    }
}

void Game::RegisterDefaultObjects()
{
    ParticleHandler::getInstance();
}

void Game::StartGame()
{
    printf("[core]         Game stated");

    remainTry = NUM_MAX_TRY;

    IsInMainMenu = false;
    IsGameStart  = true;

    BaseWidget* mainMenu = m_widgetHandler->GetWidgetByName("MainMenu");

    mainMenu->SetVisibility(false);
    mainMenu->SetActive(false);


    // Reset the widget, delete the widget and create new one in the future
    // Should have a container in WidgetHandler but need template to implement
    BaseWidget* playerHealth = m_widgetHandler->GetWidgetByName("PlayerHealth");
    playerHealth->Reset();
    playerHealth->SetActiveAndVisible();

    BaseWidget* scoreBoard = m_widgetHandler->GetWidgetByName("InGameScoreBoard");
    scoreBoard->SetActiveAndVisible();

    m_PlayerShip = new PlayerShip(this, Vec2(WORLD_CENTER_X, WORLD_CENTER_Y
                                  ), 0.f);

    //SpawnDefaultAsteroids();
    m_levelHandler->StartLevel(0, this);
    g_theAudio->StartSound(SOUND::PLAYER_SHOOTS_BULLET);
}

void Game::ReturnToMainMenu()
{
    // Clean the scene
    // TODO: Move into scene manager in SD 4
    m_levelHandler->CleanScene();
    ParticleHandler::getInstance()->CleanParticle();
    m_levelHandler->ImportLevels();

    if (IsInMainMenu == false)
    {
        IsInMainMenu = true;
        IsGameStart  = false;

        m_widgetHandler->GetWidgetByName("MainMenu")->SetActiveAndVisible();

        m_widgetHandler->GetWidgetByName("PlayerHealth")->SetInActiveAndInVisible();

        m_widgetHandler->GetWidgetByName("InGameScoreBoard")->SetInActiveAndInVisible();

        printf("[core]      Return to Main menu");
    }
}

void Game::UpdateTimerHandle(float deltaTime)
{
    for (FTimerHandle*& element : m_timerHandles)
    {
        if (element != nullptr)
            element->Update(deltaTime);
    }
}

void Game::RenderEntities() const
{
    for (Bullet* m_bullet : m_bullets)
    {
        if (m_bullet != nullptr)
        {
            m_bullet->Render();
        }
    }

    for (Asteroid* asteroid : m_asteroids)
    {
        if (asteroid != nullptr)
        {
            asteroid->Render();
        }
    }

    for (Beetle* beetle : m_entities_beetle)
    {
        if (beetle != nullptr)
            beetle->Render();
    }

    for (Wasp* wasp : m_entity_wasp)
    {
        if (wasp != nullptr)
            wasp->Render();
    }

    for (Cube* cube : m_cube)
    {
        if (cube != nullptr)
            cube->Render();
    }

    m_PlayerShip->Render();
}

void Game::HandleEntityCollisions()
{
    ParticleHandler::getInstance()->HandleParticleCollision();
    // Handle Bullet Asteroids Collision
    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet == nullptr || !m_bullet->IsAlive())
            continue;

        for (Asteroid*& asteroid : m_asteroids)
        {
            if (asteroid == nullptr || !asteroid->IsAlive())
                continue;

            if (DoDiscsOverlap(m_bullet->m_position, m_bullet->m_physicsRadius, asteroid->m_position,
                               asteroid->m_physicsRadius))
            {
                m_bullet->OnColliedEnter(asteroid);
                asteroid->OnColliedEnter(m_bullet);
                break; // Exit the loop after collision
            }
        }

        for (Beetle* beetle : m_entities_beetle)
        {
            if (beetle == nullptr || !beetle->IsAlive())
                continue;
            if (DoDiscsOverlap(m_bullet->m_position, m_bullet->m_physicsRadius, beetle->m_position,
                               beetle->m_physicsRadius))
            {
                m_bullet->OnColliedEnter(beetle);
                beetle->OnColliedEnter(m_bullet);
                break; // Exit the loop after collision
            }
        }

        for (Wasp* wasp : m_entity_wasp)
        {
            if (wasp == nullptr || !wasp->IsAlive())
                continue;
            if (DoDiscsOverlap(m_bullet->m_position, m_bullet->m_physicsRadius, wasp->m_position,
                               wasp->m_physicsRadius))
            {
                m_bullet->OnColliedEnter(wasp);
                wasp->OnColliedEnter(m_bullet);
                break; // Exit the loop after collision
            }
        }

        for (Cube* cube : m_cube)
        {
            if (cube == nullptr || !cube->IsAlive())
                continue;
            if (PushDiscOutOfAABB2D(m_bullet->m_position, m_bullet->m_physicsRadius, *cube->m_aabb))
            {
                m_bullet->OnColliedEnter(cube);
                cube->OnColliedEnter(m_bullet);
                break;
            }
        }
    }

    // ship to entity collision
    for (Asteroid*& asteroid : m_asteroids)
    {
        if (asteroid == nullptr || !asteroid->IsAlive() || !m_PlayerShip->IsAlive())
            continue;

        if (DoDiscsOverlap(asteroid->m_position, asteroid->m_physicsRadius, m_PlayerShip->m_position,
                           m_PlayerShip->m_physicsRadius))
        {
            m_PlayerShip->OnColliedEnter(asteroid);
            asteroid->OnColliedEnter(m_PlayerShip);
            g_theAudio->StartSound(SOUND::ENEMY_DAMAGED);
            break; // Exit the loop after collision
        }
    }

    for (Beetle* beetle : m_entities_beetle)
    {
        if (beetle == nullptr || !beetle->IsAlive() || !m_PlayerShip->IsAlive())
            continue;

        if (DoDiscsOverlap(beetle->m_position, beetle->m_physicsRadius, m_PlayerShip->m_position,
                           m_PlayerShip->m_physicsRadius))
        {
            m_PlayerShip->OnColliedEnter(beetle);
            beetle->OnColliedEnter(m_PlayerShip);
            g_theAudio->StartSound(SOUND::ENEMY_DAMAGED);
            break; // Exit the loop after collision
        }
    }

    for (Wasp* wasp : m_entity_wasp)
    {
        if (wasp == nullptr || !wasp->IsAlive() || !m_PlayerShip->IsAlive())
            continue;

        if (DoDiscsOverlap(wasp->m_position, wasp->m_physicsRadius, m_PlayerShip->m_position,
                           m_PlayerShip->m_physicsRadius))
        {
            m_PlayerShip->OnColliedEnter(wasp);
            wasp->OnColliedEnter(m_PlayerShip);
            g_theAudio->StartSound(SOUND::ENEMY_DAMAGED);
            break; // Exit the loop after collision
        }
    }

    for (Cube* cube : m_cube)
    {
        if (cube == nullptr || !cube->IsAlive() || !m_PlayerShip->IsAlive())
            continue;
        if (PushDiscOutOfAABB2D(m_PlayerShip->m_position, m_PlayerShip->m_physicsRadius, *cube->m_aabb))
        {
            m_PlayerShip->OnColliedEnter(cube);
            cube->OnColliedEnter(m_PlayerShip);
            g_theAudio->StartSound(SOUND::ENEMY_DAMAGED);
            break;
        }
    }
}

void Game::GarbageCollection()
{
    ParticleHandler::getInstance()->GarbageCollection();
    for (Bullet*& m_bullet : m_bullets)
    {
        if (m_bullet != nullptr && m_bullet->IsGarbage())
        {
            delete m_bullet;
            m_bullet = nullptr;
        }
    }

    for (Asteroid*& asteroid : m_asteroids)
    {
        if (asteroid != nullptr && asteroid->IsGarbage())
        {
            delete asteroid;
            asteroid = nullptr;
        }
    }

    for (Beetle*& beetle : m_entities_beetle)
    {
        if (beetle != nullptr && beetle->IsGarbage())
        {
            delete beetle;
            beetle = nullptr;
        }
    }

    for (Wasp*& wasp : m_entity_wasp)
    {
        if (wasp != nullptr && wasp->IsGarbage())
        {
            delete wasp;
            wasp = nullptr;
        }
    }

    for (Cube*& cube : m_cube)
    {
        if (cube != nullptr && cube->IsGarbage())
        {
            delete cube;
            cube = nullptr;
        }
    }
}


void Game::Spawn(EEntity entityType)
{
    switch (entityType)
    {
    case ENTITY_TYPE_BEETLE:
        Beetle* entityBeetle;
        entityBeetle             = new Beetle(this, Vec2(), 0.0f);
        entityBeetle->m_position = getRandomPositionOffscreen(entityBeetle);
        for (Beetle*& element : m_entities_beetle)
        {
            if (element == nullptr)
            {
                element = entityBeetle;
                printf("[entity]    Spawn 1 Entity - type: Beetle\n");
                break;
            }
        }
        break;
    case ENTITY_TYPE_CUBE:
        Cube* entityCube;
        entityCube             = new Cube(this, Vec2(), 0.0f);
        entityCube->m_position = Vec2(205, 95);
        for (Cube*& element : m_cube)
        {
            if (element == nullptr)
            {
                element = entityCube;
                printf("[entity]    Spawn 1 Entity - type: Cube\n");
                break;
            }
        }
        break;
    case ENTITY_TYPE_WASP:
        Wasp* entityWasp;
        entityWasp             = new Wasp(this, Vec2(), 0.0f);
        entityWasp->m_position = getRandomPositionOffscreen(entityWasp);
        for (Wasp*& element : m_entity_wasp)
        {
            if (element == nullptr)
            {
                element = entityWasp;
                printf("[entity]    Spawn 1 Entity - type: Wasp\n");
                break;
            }
        }
        break;
    default: ;
    }
}

void Game::Spawn(EEntity entityType, int amount)
{
    for (int i = 0; i < amount; i++)
    {
        Spawn(entityType);
    }
}


void Game::OnEntityDieEvent(Entity* entity)
{
    printf(
        "[event]     On entity die event triggered\n"
        "            > Instigator: %s\n", typeid(*entity).name());
}

void Game::OnPlayerShipRespawnEvent(PlayerShip* playerShip, int remainTry)
{
    printf(
        "[event]     PlayerShip respawn event triggered\n"
        "            > Remain tries: %d\n", remainTry);
    g_theAudio->StartSound(SOUND::PLAYER_RESPAWN);

    static_cast<WidgetPlayerHealth*>(m_widgetHandler->GetWidgetByName("PlayerHealth"))->OnPlayerShipRespawn(
        playerShip, remainTry);
}

void Game::OnPlayerShipDeathEvent(PlayerShip* playerShip)
{
    printf(
        "[event]     PlayerShip death event triggered\n");

    g_theAudio->StartSound(SOUND::PLAYER_DIES);
    m_worldCamera->DoShakeEffect(Vec2(3, 3), 1.0f, true);
    if (remainTry - 1 == 0) // because on spawn will deduct remain try
    {
        SetTimer(3, nullptr);
    }
}

void Game::OnMainMenuDisplayEvent()
{
}

void Game::OnPointGainEvent(int gainedScore)
{
    Score += gainedScore;
    g_theAudio->StartSound(SOUND::POINT_ADD);
}

FTimerHandle* Game::SetTimer(float timer, void (*callback)())
{
    auto handle = new FTimerHandle();
    handle->SetOwner(this);
    handle->times            = timer;
    handle->onFinishCallback = callback;
    for (FTimerHandle*& element : m_timerHandles)
    {
        if (element == nullptr)
        {
            element = handle;
            return element;
        }
    }
    return nullptr;
}
