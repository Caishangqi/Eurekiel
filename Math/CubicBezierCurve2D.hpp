#pragma once
#include <vector>

#include "Engine/Math/Vec2.hpp"

class CubicHermiteCurve2D;

class CubicBezierCurve2D
{
public:
    CubicBezierCurve2D(Vec2 startPos, Vec2 guidePos1, Vec2 guidePos2, Vec2 endPos);
    CubicBezierCurve2D();
    explicit          CubicBezierCurve2D(CubicHermiteCurve2D const& fromHermite);
    Vec2              EvaluateAtParametric(float parametricZeroToOne) const;
    Vec2              EvaluateAtApproximateDistance(float distanceAlongCurve, int numSubdivisions = 64) const;
    float             GetLength(int numSubdivisions = 64) const;
    std::vector<Vec2> GetPoints() const;

public:
    Vec2 m_startPos;
    Vec2 m_guidePos1;
    Vec2 m_guidePos2;
    Vec2 m_endPos;
};

class CubicHermiteCurve2D
{
public:
    CubicHermiteCurve2D(Vec2 startPos, Vec2 velocityU, Vec2 velocityV, Vec2 endPos);
    explicit CubicHermiteCurve2D(CubicBezierCurve2D const& fromBezier);
    Vec2     EvaluateAtParametric(float parametricZeroToOne) const;
    Vec2     EvaluateAtApproximateDistance(float distanceAlongCurve, int numSubdivisions = 64) const;
    float    GetLength(int numSubdivisions = 64) const;

public:
    Vec2 m_startPos;
    Vec2 m_velocityU;
    Vec2 m_velocityV;
    Vec2 m_endPos;
};
