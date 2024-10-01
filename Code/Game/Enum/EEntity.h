#pragma once

enum EEntity
{
    ENTITY_TYPE_ASTEROID,
    ENTITY_TYPE_BEETLE,
    ENTITY_TYPE_BULLET,
    ENTITY_TYPE_PLAYER_SHIP,
    ENTITY_TYPE_WASP,
};

inline const char* to_string(EEntity e)
{
    switch (e)
    {
    case ENTITY_TYPE_ASTEROID: return "ASTEROID";
    case ENTITY_TYPE_BEETLE: return "BEETLE";
    case ENTITY_TYPE_BULLET: return "BULLET";
    case ENTITY_TYPE_PLAYER_SHIP: return "PLAYER_SHIP";
    case ENTITY_TYPE_WASP: return "WASP";
    default: return "unknown";
    }
}
