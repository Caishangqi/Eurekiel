#pragma once

enum EEntity
{
    ASTEROID,
    BEETLE,
    BULLET,
    PLAYER_SHIP,
    WASP,
};

inline const char* to_string(EEntity e)
{
    switch (e)
    {
    case ASTEROID: return "ASTEROID";
    case BEETLE: return "BEETLE";
    case BULLET: return "BULLET";
    case PLAYER_SHIP: return "PLAYER_SHIP";
    case WASP: return "WASP";
    default: return "unknown";
    }
}
