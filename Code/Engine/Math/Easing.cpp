#include "Easing.hpp"

#include "Bezier.hpp"
#include "Engine/Math/MathUtils.hpp"

float SmoothStart2(float inValue)
{
    return inValue * inValue;
}

float SmoothStart3(float inValue)
{
    return inValue * inValue * inValue;
}

float SmoothStart4(float inValue)
{
    return inValue * inValue * inValue * inValue;
}

float SmoothStart5(float inValue)
{
    return inValue * inValue * inValue * inValue * inValue;
}

float SmoothStart6(float inValue)
{
    return inValue * inValue * inValue * inValue * inValue * inValue;
}

float SmoothStop2(float inValue)
{
    float oneMinusT = 1.0f - inValue;
    return 1 - oneMinusT * oneMinusT;
}

float SmoothStop3(float inValue)
{
    float oneMinusT = 1.0f - inValue;
    return 1 - oneMinusT * oneMinusT * oneMinusT;
}

float SmoothStop4(float inValue)
{
    float oneMinusT = 1.0f - inValue;
    return 1 - oneMinusT * oneMinusT * oneMinusT * oneMinusT;
}

float SmoothStop5(float inValue)
{
    float oneMinusT = 1.0f - inValue;
    return 1 - oneMinusT * oneMinusT * oneMinusT * oneMinusT * oneMinusT;
}

float SmoothStop6(float inValue)
{
    float oneMinusT = 1.0f - inValue;
    return 1 - oneMinusT * oneMinusT * oneMinusT * oneMinusT * oneMinusT * oneMinusT;
}

float SmoothStep3(float inValue)
{
    return Interpolate(SmoothStart3(inValue), SmoothStop3(inValue), inValue);
}

float SmoothStep5(float inValue)
{
    return Interpolate(SmoothStart5(inValue), SmoothStop5(inValue), inValue);
}

float Hesitate3(float inValue)
{
    return ComputeCubicBezier1D(0, 1, 0, 1, inValue);
}

float Hesitate5(float inValue)
{
    return ComputeQuinticBezier1D(0, 1, 0, 1, 0, 1, inValue);
}

float CustomEasingCaizii(float inValue)
{
    return inValue * inValue;
}

/// Can be anything you want; does’t need to match the Demo.  Should be weird/interesting in some way.  Should return 0 for an input of 0,
/// and 1 for an input of 1 (i.e. F(0)=0 and F(1)=1), but do something unique and nonlinear in-between.  See Demo for one possible example.
/// Can be written in game code (in MathVisualTests) instead of Engine.
/// @param inValue 
/// @param easingFunc 
/// @return result
float CustomEasing(float inValue, float (*easingFunc)(float))
{
    float result = easingFunc(inValue);
    return result;
}
