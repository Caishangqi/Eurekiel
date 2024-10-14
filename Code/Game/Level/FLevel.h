#pragma once
#include "Game/Enum/ELevelState.h"

enum EEntity : int;
class Game;

struct FLevel
{
private:
    Game* m_Game;

    int GetAliveEntityByType(EEntity entityType);

public:
    int         levelID           = 0;
    ELevelState state             = ELevelState::PENDING;
    int         levelAmountBeetle = 1;
    int         levelAmountWasp   = 1;
    int         goalScore         = 0;

    FLevel();
    FLevel(int amountBeetle, int amountWasp);
    FLevel(int levelID, int amountBeetle, int amountWasp, int goalScore);
    FLevel(int levelID, int amountBeetle, int amountWasp);
    ~FLevel();
    FLevel(const FLevel& other);

    FLevel& SetGameInstance(Game* game);

    void OnStart();

    void OnEnd();

    void OnUpdate(float deltaSecond);

    FLevel(FLevel& other) noexcept
        : m_Game(other.m_Game), levelID(other.levelID),
          state(other.state),
          levelAmountBeetle(other.levelAmountBeetle),
          levelAmountWasp(other.levelAmountWasp)
    {
    }

    bool CheckComplete();

    FLevel& operator=(const FLevel& other);

    friend bool operator==(const FLevel& lhs, const FLevel& rhs);

    friend bool operator!=(const FLevel& lhs, const FLevel& rhs);
};
