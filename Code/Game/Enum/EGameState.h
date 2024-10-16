#pragma once

enum class EGameState
{
    MENU, // In the main menu state
    PAUSE, // In the Pause state
    TUTORIAL, // On the tutorial state
    INLEVEL // In the level state
};

inline const char* to_string(EGameState e)
{
    switch (e)
    {
    case EGameState::MENU: return "MENU";
    case EGameState::PAUSE: return "PAUSE";
    case EGameState::TUTORIAL: return "TUTORIAL";
    case EGameState::INLEVEL: return "INLEVEL";
    default: return "unknown";
    }
}
