#pragma once
enum EShape
{
    TRIANGLE,
    RECTANGLE,
    PIXEL
};

inline const char* to_string(EShape e)
{
    switch (e)
    {
    case EShape::TRIANGLE: return "TRIANGLE";
    case EShape::RECTANGLE: return "RECTANGLE";
    case EShape::PIXEL: return "PIXEL";
    default: return "unknown";
    }
}
