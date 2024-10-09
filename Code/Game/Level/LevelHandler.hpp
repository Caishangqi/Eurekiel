#pragma once
#include "FLevel.h"

class Game;
class Entity;

class LevelHandler
{
public:
    LevelHandler(Game* owner);
    ~LevelHandler();

    void Update(float deltaTime);

    void StartLevel(FLevel* level, Game* gameInstance);
    void StartLevel(int id, Game* gameInstance);
    void RunNextLevel();

    void CompleteLevel(FLevel& level);
    void InterruptLevel(FLevel& level);

    /**
     * Clean registered entity in the game, this method will
     * move to scene manager in the future
     */
    void CleanScene();

    // Event
    void OnEntityDie(Entity* entity);
    void ImportLevels();

private:
    FLevel* m_levels[5] = {};
    Game*   m_game      = nullptr;
    FLevel* m_currentLevel;
};
