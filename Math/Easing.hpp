#pragma once

/// All functions should take a single float argument (usually in [0,1]) and return a single float argument (usually also in [0,1]).

#define EaseInQuadratic SmoothStart2
#define EaseInCubic SmoothStart3
#define EaseInQuartic SmoothStart4
#define EaseInQuintic SmoothStart5
#define EaseIn6thOrder SmoothStart6

#define EaseOutQuadratic SmoothStop2
#define EaseOutCubic SmoothStop3
#define EaseOutQuartic SmoothStop4
#define EaseOutQuintic SmoothStop5
#define EaseOut6thOrder SmoothStop6

#define SmoothStep SmoothStep3
#define SmootherStep SmoothStep5

typedef float (*EaseFuncPtr)(float);

float EaseInQuadratic(float inValue);
float EaseInCubic(float inValue);
float EaseInQuartic(float inValue);
float EaseInQuintic(float inValue);
float EaseIn6thOrder(float inValue);

float EaseOutQuadratic(float inValue);
float EaseOutCubic(float inValue);
float EaseOutQuartic(float inValue);
float EaseOutQuintic(float inValue);
float EaseOut6thOrder(float inValue);

float SmoothStep(float inValue);
float SmootherStep(float inValue);

float Hesitate3(float inValue);
float Hesitate5(float inValue);

float CustomEasingCaizii(float inValue);
float CustomEasing(float inValue, float (*easingFunc)(float));
