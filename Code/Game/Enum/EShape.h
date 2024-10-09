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
    case TRIANGLE: return "TRIANGLE";
    case RECTANGLE: return "RECTANGLE";
    case PIXEL: return "PIXEL";
    default: return "unknown";
    }
}
