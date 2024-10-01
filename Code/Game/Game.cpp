#include "Game.hpp"

#include <functional>
#include <vcruntime_typeinfo.h>

#include "App.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Entity/Beetle.hpp"
#include "Entity/Wasp.hpp"
#include "Enum/EEntity.h"
#include "Level/LevelHandler.hpp"
#include "Particle/ParticleHandler.hpp"
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
}

Game::~Game()
{
    delete m_PlayerShip;
    m_PlayerShip = nullptr;

    delete m_levelHandler;
    m_levelHandler = nullptr;

    delete m_widgetHandler;
    m_widgetHandler = nullptr;

    ParticleHandler::getInstance()->CleanParticle();
}


void Game::Render() const
{
    if (IsGameStart)
    {
        RenderEntities();
    }

    ParticleHandler::getInstance()->Render();
    m_widgetHandler->Render();
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
    for (Asteroid*& m_asteroid : m_asteroid)
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

void Game::Update(float deltaTime)
{
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

        HandleEntityCollisions();
    }


    m_levelHandler->Update(deltaTime);

    HandleKeyBoardEvent(deltaTime);

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
    ERROR_RECOVERABLE("MAX AMOUNT OF ENTITY REACH")
}

void Game::SpawnDefaultAsteroids()
{
    for (int numberAsteroid = 0; numberAsteroid < NUM_STARTING_ASTEROIDS; ++numberAsteroid)
    {
        // Generate random position in world 
        float     worldPositionX    = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_X);
        float     worldPositionY    = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_Y);
        float     randomOrientation = g_rng->RollRandomFloatInRange(0, 360);
        Asteroid* AddedAsteroid     = new Asteroid(this, Vec2(worldPositionX, worldPositionY), randomOrientation);
        Vec2      offScreenPos      = getRandomPositionOffscreen(AddedAsteroid);
        AddedAsteroid->m_position   = offScreenPos;
        m_asteroid[numberAsteroid]  = AddedAsteroid;
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
        if (m_asteroid[numberAsteroid] == nullptr)
        {
            // Generate random position in world 
            float worldPositionX       = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_X);
            float worldPositionY       = g_rng->RollRandomFloatInRange(0, WORLD_SIZE_Y);
            float randomOrientation    = g_rng->RollRandomFloatInRange(0, 360);
            m_asteroid[numberAsteroid] = new Asteroid(this, Vec2(worldPositionX, worldPositionY), randomOrientation);
            return;
        }
    }
    // Which means reach the max of spawning
    ERROR_RECOVERABLE("MAX AMOUNT OF ENTITY REACH")
}

void Game::HandleKeyBoardEvent(float deltaTime)
{
    XboxController const& controller = g_theInput->GetController(0);
    if (IsInMainMenu)
    {
        bool spaceBarPressed = g_theInput->WasKeyJustPressed(32) || controller.WasButtonJustPressed(XBOX_BUTTON_A);
        bool NKeyPressed     = g_theInput->WasKeyJustPressed('N') || controller.WasButtonJustPressed(XBOX_BUTTON_START);
        if (spaceBarPressed || NKeyPressed)
        {
            StartGame();
        }
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

    m_widgetHandler->mainMenuWidget->SetVisibility(false);
    m_widgetHandler->mainMenuWidget->SetActive(false);


    // Reset the widget, delete the widget and create new one in the future
    // Should have a container in WidgetHandler but need template to implement
    m_widgetHandler->playerHealthWidget->Reset();
    m_widgetHandler->playerHealthWidget->SetActiveAndVisible();

    m_PlayerShip = new PlayerShip(this, Vec2(WORLD_CENTER_X, WORLD_CENTER_Y
                                  ), 0.f);

    SpawnDefaultAsteroids();
    m_levelHandler->StartLevel(0, this);
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

        m_widgetHandler->mainMenuWidget->SetActiveAndVisible();

        m_widgetHandler->playerHealthWidget->SetActive(false);
        m_widgetHandler->playerHealthWidget->SetVisibility(false);

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

    for (Asteroid* asteroid : m_asteroid)
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

        for (Asteroid*& asteroid : m_asteroid)
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
    }

    // ship to entity collision
    for (Asteroid*& asteroid : m_asteroid)
    {
        if (asteroid == nullptr || !asteroid->IsAlive() || !m_PlayerShip->IsAlive())
            continue;

        if (DoDiscsOverlap(asteroid->m_position, asteroid->m_physicsRadius, m_PlayerShip->m_position,
                           m_PlayerShip->m_physicsRadius))
        {
            m_PlayerShip->OnColliedEnter(asteroid);
            asteroid->OnColliedEnter(m_PlayerShip);
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
            break; // Exit the loop after collision
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

    for (Asteroid*& asteroid : m_asteroid)
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
    m_widgetHandler->playerHealthWidget->OnPlayerShipRespawn(playerShip, remainTry);
}

void Game::OnPlayerShipDeathEvent(PlayerShip* playerShip)
{
    printf(
        "[event]     PlayerShip death event triggered\n");
    if (remainTry - 1 == 0) // because on spawn will deduct remain try
    {
        SetTimer(3, nullptr);
    }
}

FTimerHandle* Game::SetTimer(float timer, void (*callback)())
{
    FTimerHandle* handle = new FTimerHandle();
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
