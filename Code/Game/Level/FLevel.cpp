#include "FLevel.h"

#include "Game/Game.hpp"
#include "Game/Enum/EEntity.h"
#include "stdio.h"
#include "Game/Entity/Beetle.hpp"
#include "Game/Entity/Wasp.hpp"

int FLevel::GetAliveEntityByType(EEntity entityType)
{
    int amount = 0;
    switch (entityType)
    {
    case ENTITY_TYPE_WASP:
        for (Wasp* element : m_Game->m_entity_wasp)
        {
            if (element != nullptr && element->IsAlive())
            {
                amount++;
            }
        }
        break;
    case ENTITY_TYPE_BEETLE:
        for (Beetle* element : m_Game->m_entities_beetle)
        {
            if (element != nullptr && element->IsAlive())
            {
                amount++;
            }
        }
        break;
    default: ;
    }
    return amount;
}

FLevel::FLevel()
{
    m_Game            = nullptr;
    this->state       = ELevelState::PENDING;
    levelAmountBeetle = 0;
    levelAmountWasp   = 0;
    levelID           = -1;
}

FLevel::FLevel(int amountBeetle, int amountWasp)
{
    m_Game                  = nullptr;
    this->state             = ELevelState::PENDING;
    this->levelAmountBeetle = amountBeetle;
    this->levelAmountWasp   = amountWasp;
}

FLevel::FLevel(int levelID, int amountBeetle, int amountWasp)
{
    m_Game                  = nullptr;
    this->state             = ELevelState::PENDING;
    this->levelID           = levelID;
    this->levelAmountBeetle = amountBeetle;
    this->levelAmountWasp   = amountWasp;
}

FLevel::~FLevel()
{
    m_Game = nullptr;
}


FLevel::FLevel(const FLevel& other)
    : m_Game(other.m_Game), levelID(other.levelID),
      state(other.state),
      levelAmountBeetle(other.levelAmountBeetle),
      levelAmountWasp(other.levelAmountWasp)
{
    // 这里可能需要根据需求深拷贝 Game* 对象
}

FLevel& FLevel::SetGameInstance(Game* game)
{
    m_Game = game;
    return *this;
}


void FLevel::OnStart()
{
    printf("[level]     Start Level: %d\n", levelID);
    m_Game->Spawn(ENTITY_TYPE_WASP, levelAmountWasp);
    m_Game->Spawn(ENTITY_TYPE_BEETLE, levelAmountBeetle);
    state = ELevelState::ONGOING;
}

void FLevel::OnEnd()
{
    printf("[level]     Finish Level: %d\n", levelID);
}

void FLevel::OnUpdate(float deltaSecond)
{
    if (CheckComplete())
    {
        state = ELevelState::FINISHED;
    }
}


bool FLevel::CheckComplete()
{
    int waspAmount   = GetAliveEntityByType(ENTITY_TYPE_WASP);
    int beetleAmount = GetAliveEntityByType(ENTITY_TYPE_BEETLE);
    if ((waspAmount <= 0) && (beetleAmount <= 0))
    {
        return true;
    }
    return false;
}

FLevel& FLevel::operator=(const FLevel& other)
{
    if (this != &other)
    {
        // 防止自我赋值
        m_Game            = other.m_Game; // 注意指针的管理
        levelID           = other.levelID;
        state             = other.state;
        levelAmountBeetle = other.levelAmountBeetle;
        levelAmountWasp   = other.levelAmountWasp;
    }
    return *this;
}


bool operator==(const FLevel& lhs, const FLevel& rhs)
{
    return lhs.levelID == rhs.levelID &&
        lhs.state == rhs.state &&
        lhs.levelAmountBeetle == rhs.levelAmountBeetle &&
        lhs.levelAmountWasp == rhs.levelAmountWasp; // 这里有重复，修正为一次
}

bool operator!=(const FLevel& lhs, const FLevel& rhs)
{
    return !(lhs == rhs);
}
