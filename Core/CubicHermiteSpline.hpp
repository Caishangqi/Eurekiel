#pragma once
#include <vector>

#include "Engine/Math/CubicBezierCurve2D.hpp"
#include "Engine/Math/Vec2.hpp"

class CubicHermiteSpline
{
public:
    CubicHermiteSpline() = default;
    CubicHermiteSpline(std::vector<Vec2> points);

public:
    std::vector<Vec2>&                GetPoints();
    Vec2                              EvaluateAtParametric(float parametricZeroToNumCurvesSections) const;
    Vec2                              EvaluateAtApproximateDistance(float distanceAlongCurve, int numSubdivisions = 64) const;
    float                             GetLength(int numSubdivisions = 64) const;
    int                               GetNumOfCurves() const;
    std::vector<CubicHermiteCurve2D>& GetCurves();

private:
    std::vector<Vec2>                m_points;
    std::vector<CubicHermiteCurve2D> m_curves;
    float                            m_length = 0.f;
};
