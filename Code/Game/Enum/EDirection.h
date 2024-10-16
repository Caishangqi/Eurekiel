#pragma once

enum class EDirection
{
    LEFT,
    RIGHT,
    UP,
    DOWN
};

inline const char* to_string(EDirection e)
{
    switch (e)
    {
    case EDirection::LEFT: return "LEFT";
    case EDirection::RIGHT: return "RIGHT";
    case EDirection::UP: return "UP";
    case EDirection::DOWN: return "DOWN";
    default: return "INVALID";
    }
}
