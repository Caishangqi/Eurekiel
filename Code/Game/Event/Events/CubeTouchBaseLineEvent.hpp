#pragma once
#include "Game/Entity/Cube.hpp"
#include "Game/Event/Event.hpp"

struct CubeTouchBaseLineEvent : public TypedEvent<CubeTouchBaseLineEvent>
{
public:
    Cube* cube = nullptr;

    CubeTouchBaseLineEvent(Cube* touchedCube) : cube(touchedCube)
    {
    }
};
