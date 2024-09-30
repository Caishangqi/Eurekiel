#pragma once

enum class ELevelState
{
    PENDING, // Ready for server
    ONGOING, // Current level is initialize and had spawn entities
    FINISHED, // Player killed all entity that the level specific
    INACTIVE // deactivate and not update in game loop;
    
    
};

inline const char* to_string(ELevelState e)
{
    switch (e)
    {
    case ELevelState::PENDING: return "PENDING";
    case ELevelState::ONGOING: return "ONGOING";
    case ELevelState::FINISHED: return "FINISHED";
    case ELevelState::INACTIVE: return "INACTIVE";
    default: return "unknown";
    }
}

