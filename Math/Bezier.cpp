#include "Bezier.hpp"

#include "MathUtils.hpp"

/// where A,B,C,D are the Cubic (3rd order) Bezier control points (A is the start pos, and D is the end pos), and t is the parametric in [0,1]
/// These functions are particularly useful in deriving, and/or writing, several of the advanced Easing Functions (see below). 
/// @param A 
/// @param B 
/// @param C 
/// @param D 
/// @param t 
/// @return 
float ComputeCubicBezier1D(float A, float B, float C, float D, float t)
{
    float interpolateAB = Interpolate(A, B, t);
    float interpolateBC = Interpolate(B, C, t);
    float interpolateCD = Interpolate(C, D, t);

    float interpolateABC = Interpolate(interpolateAB, interpolateBC, t);
    float interpolateBCD = Interpolate(interpolateBC, interpolateCD, t);

    return Interpolate(interpolateABC, interpolateBCD, t);

    /// Expanded formular
    /*float oneMinusT = 1.0f - t;
    return (oneMinusT * oneMinusT * oneMinusT * A)
        + (3.0f * oneMinusT * oneMinusT * t * B)
        + (3.0f * oneMinusT * t * t * C)
        + (t * t * t * D);*/
}

/// where A,B,C,D,E,F are the Quintic (5th order) Bezier control points (A is the start, and F is the end), and t is the parametric in [0,1].
/// These functions are particularly useful in deriving, and/or writing, several of the advanced Easing Functions (see below). 
/// @param A 
/// @param B 
/// @param C 
/// @param D 
/// @param E 
/// @param F 
/// @param t 
/// @return 
float ComputeQuinticBezier1D(float A, float B, float C, float D, float E, float F, float t)
{
    float oneMinusT = 1.0f - t;

    // Apply the 5th-order Bezier formula
    return (oneMinusT * oneMinusT * oneMinusT * oneMinusT * oneMinusT * A) +
        (5.0f * oneMinusT * oneMinusT * oneMinusT * oneMinusT * t * B) +
        (10.0f * oneMinusT * oneMinusT * oneMinusT * t * t * C) +
        (10.0f * oneMinusT * oneMinusT * t * t * t * D) +
        (5.0f * oneMinusT * t * t * t * t * E) +
        (t * t * t * t * t * F);
}
