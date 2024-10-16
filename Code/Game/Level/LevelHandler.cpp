#include "LevelHandler.hpp"
#include <iostream>
#include "FLevel.h"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Game/Game.hpp"
#include "Game/Entity/Asteroid.hpp"
#include "Game/Entity/Beetle.hpp"
#include "Game/Entity/Bullet.hpp"
#include "Game/Entity/PlayerShip.hpp"
#include "Game/Entity/Wasp.hpp"
#include "Game/Entity/Tetromino/ITetromino.hpp"
#include "Game/Entity/Tetromino/JTetromino.hpp"
#include "Game/Entity/Tetromino/LTetromino.h"
#include "Game/Entity/Tetromino/OTetromino.hpp"
#include "Game/Entity/Tetromino/STetromino.hpp"
#include "Game/Entity/Tetromino/TTetromino.hpp"
#include "Game/Entity/Tetromino/ZTetromino.hpp"
#include "Game/Event/EventManager.hpp"
#include "Game/Event/Events/CubeTouchBaseLineEvent.hpp"
#include "Game/Event/Events/TetrominoAllChildDieEvent.hpp"
#include "Game/Grid/Grid.hpp"

LevelHandler::LevelHandler(Game* owner)
{
    printf("[level]     Initialize level system!\n");
    m_game = owner;
    ImportLevels();
}

// nulled pointer but do not delete m_game
LevelHandler::~LevelHandler()
{
    // 释放所有的 FLevel 指针
    for (int i = 0; i < 5; ++i)
    {
        delete m_levels[i];
        m_levels[i] = nullptr;
    }
    m_game = nullptr;
}

void LevelHandler::StartLevel(int id, Game* gameInstance)
{
    m_currentLevel = m_levels[id]; // 指向当前关卡
    m_currentLevel->SetGameInstance(m_game);
    m_currentLevel->OnStart(); // 启动当前关卡

    if (id == 0)
    {
        GenerateRandomTetromino(); // Spawn the first Tetramino
    }
}

void LevelHandler::StartLevel(FLevel* level, Game* gameInstance)
{
    m_currentLevel = level;
    m_currentLevel->SetGameInstance(m_game);
    m_currentLevel->OnStart();
}

void LevelHandler::Update(float deltaTime)
{
    if (m_currentLevel != nullptr)
    {
        if (m_currentLevel->state == ELevelState::PENDING)
        {
            return;
        }
        if (m_currentLevel->state == ELevelState::ONGOING)
        {
            m_currentLevel->OnUpdate(deltaTime);
        }
        if (m_currentLevel->state == ELevelState::FINISHED)
        {
            m_currentLevel->OnEnd();
            m_currentLevel->state = ELevelState::INACTIVE;
            RunNextLevel();
        }
    }
}


void LevelHandler::RunNextLevel()
{
    for (int i = 0; i < 5; i++)
    {
        if (m_levels[i] == m_currentLevel)
        {
            if (i + 1 < 5)
            {
                StartLevel(m_levels[i + 1], m_game);
                return;
            }
            printf("[level]     Completed All levels");
            // use return to menu to call Reimport levels and clean scene
            m_game->SetTimer(3.0, nullptr);
            return;
        }
    }
}

void LevelHandler::CompleteLevel(FLevel& level)
{
}

void LevelHandler::InterruptLevel(FLevel& level)
{
}

FLevel* LevelHandler::GetCurrentLevel()
{
    return m_currentLevel;
}

void LevelHandler::CleanScene()
{
    for (Asteroid*& m_asteroid : m_game->m_asteroids)
    {
        if (m_asteroid != nullptr)
        {
            delete m_asteroid;
            m_asteroid = nullptr;
        }
    }

    for (Bullet*& m_bullet : m_game->m_bullets)
    {
        if (m_bullet != nullptr)
        {
            delete m_bullet;
            m_bullet = nullptr;
        }
    }

    for (Beetle*& m_entities_beetle : m_game->m_entities_beetle)
    {
        if (m_entities_beetle != nullptr)
        {
            delete m_entities_beetle;
            m_entities_beetle = nullptr;
        }
    }

    for (Wasp*& m_entity_wasp : m_game->m_entity_wasp)
    {
        if (m_entity_wasp != nullptr)
        {
            delete m_entity_wasp;
            m_entity_wasp = nullptr;
        }
    }

    for (Cube*& m_cube : m_game->m_cube)
    {
        if (m_cube != nullptr)
        {
            delete m_cube;
            m_cube = nullptr;
        }
    }

    delete m_game->m_PlayerShip;
    m_game->m_PlayerShip = nullptr;
}

void LevelHandler::GenerateRandomTetromino()
{
    int            index               = g_rng->RollRandomIntInRange(0, 6);
    int            randomSpawnPosition = g_rng->RollRandomIntInRange(3, 19);
    BaseTetromino* bt                  = nullptr;
    switch (index)
    {
    case 0:
        bt = new ITetromino(IntVec2(39, randomSpawnPosition));
        bt->SetParentGrid(m_game->m_grid);
        m_game->m_grid->PlaceTetromino(bt);
        break;
    case 1:
        bt = new JTetromino(IntVec2(39, randomSpawnPosition));
        bt->SetParentGrid(m_game->m_grid);
        m_game->m_grid->PlaceTetromino(bt);
        break;
    case 2:
        bt = new LTetromino(IntVec2(39, randomSpawnPosition));
        bt->SetParentGrid(m_game->m_grid);
        m_game->m_grid->PlaceTetromino(bt);
        break;
    case 3:
        bt = new OTetromino(IntVec2(39, randomSpawnPosition));
        bt->SetParentGrid(m_game->m_grid);
        m_game->m_grid->PlaceTetromino(bt);
        break;
    case 4:
        bt = new STetromino(IntVec2(39, randomSpawnPosition));
        bt->SetParentGrid(m_game->m_grid);
        m_game->m_grid->PlaceTetromino(bt);
        break;
    case 5:
        bt = new TTetromino(IntVec2(39, randomSpawnPosition));
        bt->SetParentGrid(m_game->m_grid);
        m_game->m_grid->PlaceTetromino(bt);
        break;
    case 6:
        bt = new ZTetromino(IntVec2(39, randomSpawnPosition));
        bt->SetParentGrid(m_game->m_grid);
        m_game->m_grid->PlaceTetromino(bt);
        break; /*
    case 7:
        for (int i = 0; i < GRID_HEIGHT_SIZE; i++)
        {
            m_game->m_grid->PlaceCube(IntVec2(39, i));
        }
        break;*/
    default:
        printf("[level]     Invalid index!\n");
    }
}

void LevelHandler::OnEntityDie(Entity* entity)
{
}

void LevelHandler::ImportLevels()
{
    delete m_currentLevel;
    m_currentLevel = nullptr;

    // On Heap 
    m_levels[0] = new FLevel(1, 1, 0, 100);

    m_levels[1] = new FLevel(2, 0, 1, 300);

    m_levels[2] = new FLevel(3, 1, 1, 600);

    m_levels[3] = new FLevel(4, 1, 1, 1200);

    m_levels[4] = new FLevel(5, 1, 1, 1500);

    for (int i = 0; i < 5; ++i)
    {
        printf("[level]     Load Level: %d\n", m_levels[i]->levelID);
    }
}
