#include "LevelHandler.hpp"
#include <iostream>
#include "FLevel.h"
#include "Game/Game.hpp"
#include "Game/Entity/Beetle.hpp"
#include "Game/Entity/Wasp.hpp"

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

void LevelHandler::CleanScene()
{
    for (Asteroid*& m_asteroid : m_game->m_asteroid)
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

    delete m_game->m_PlayerShip;
    m_game->m_PlayerShip = nullptr;
}

void LevelHandler::ImportLevels()
{
    m_currentLevel = nullptr;

    // On Heap 
    m_levels[0] = new FLevel(1, 1, 0);

    m_levels[1] = new FLevel(2, 0, 1);

    m_levels[2] = new FLevel(3, 1, 1);

    m_levels[3] = new FLevel(4, 1, 1);

    m_levels[4] = new FLevel(5, 1, 1);

    for (int i = 0; i < 5; ++i)
    {
        printf("[level]     Load Level: %d\n", m_levels[i]->levelID);
    }
}
