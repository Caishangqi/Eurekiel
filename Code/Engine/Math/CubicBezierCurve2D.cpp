#include "CubicBezierCurve2D.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Core/EngineCommon.hpp"

CubicBezierCurve2D::CubicBezierCurve2D(Vec2 startPos, Vec2 guidePos1, Vec2 guidePos2, Vec2 endPos): m_startPos(startPos), m_guidePos1(guidePos1), m_guidePos2(guidePos2), m_endPos(endPos)
{
}

CubicBezierCurve2D::CubicBezierCurve2D()
{
}

CubicBezierCurve2D::CubicBezierCurve2D(const CubicHermiteCurve2D& fromHermite)
{
    m_startPos  = fromHermite.m_startPos;
    m_guidePos1 = fromHermite.m_startPos + (fromHermite.m_velocityU / 3.f);
    m_guidePos2 = fromHermite.m_endPos - (fromHermite.m_velocityV / 3.f);
    m_endPos    = fromHermite.m_endPos;
}

Vec2 CubicBezierCurve2D::EvaluateAtParametric(float parametricZeroToOne) const
{
    Vec2 interpolatedStartPosGuidePos1  = Interpolate(m_startPos, m_guidePos1, parametricZeroToOne);
    Vec2 interpolatedGuidePos1GuidePos2 = Interpolate(m_guidePos1, m_guidePos2, parametricZeroToOne);
    Vec2 interpolatedGuidePos2EndPos    = Interpolate(m_guidePos2, m_endPos, parametricZeroToOne);

    Vec2 _interp_0 = Interpolate(interpolatedStartPosGuidePos1, interpolatedGuidePos1GuidePos2, parametricZeroToOne);
    Vec2 _interp_1 = Interpolate(interpolatedGuidePos1GuidePos2, interpolatedGuidePos2EndPos, parametricZeroToOne);


    return Interpolate(_interp_0, _interp_1, parametricZeroToOne);
}

Vec2 CubicBezierCurve2D::EvaluateAtApproximateDistance(float distanceAlongCurve, int numSubdivisions) const
{
    float totalLength = GetLength(numSubdivisions);
    if (distanceAlongCurve <= 0.f) return m_startPos;
    if (distanceAlongCurve >= totalLength) return m_endPos;
    Vec2  prevPos  = EvaluateAtParametric(0.f);
    float traveled = 0.f;
    float step     = 1.f / static_cast<float>(numSubdivisions);

    for (int i = 1; i <= numSubdivisions; ++i)
    {
        float t             = step * static_cast<float>(i);
        Vec2  currPos       = EvaluateAtParametric(t);
        float segmentLength = (currPos - prevPos).GetLength();

        // Check if the target distance falls within this segment
        if (distanceAlongCurve <= traveled + segmentLength)
        {
            // fraction = how far we need to move along this small segment
            float fraction = (distanceAlongCurve - traveled) / segmentLength;
            // Do a linear interpolation between prevPos and currPos
            return Interpolate(prevPos, currPos, fraction);
        }

        traveled += segmentLength;
        prevPos = currPos;
    }

    // If floating-point rounding leads us here, just return end point
    return m_endPos;
}

float CubicBezierCurve2D::GetLength(int numSubdivisions) const
{
    float totalLength = 0.f;
    Vec2  prevPos     = EvaluateAtParametric(0.f);
    float step        = 1.f / static_cast<float>(numSubdivisions);
    for (int i = 1; i <= numSubdivisions; ++i)
    {
        float t   = step * static_cast<float>(i);
        Vec2  pos = EvaluateAtParametric(t);
        totalLength += (pos - prevPos).GetLength();
        prevPos = pos;
    }
    return totalLength;
}

std::vector<Vec2> CubicBezierCurve2D::GetPoints() const
{
    std::vector<Vec2> points;
    points.reserve(4);
    points.push_back(m_startPos);
    points.push_back(m_guidePos1);
    points.push_back(m_guidePos2);
    points.push_back(m_endPos);
    return points;
}


CubicHermiteCurve2D::CubicHermiteCurve2D(Vec2 startPos, Vec2 velocityU, Vec2 velocityV, Vec2 endPos): m_startPos(startPos), m_velocityU(velocityU), m_velocityV(velocityV), m_endPos(endPos)
{
}

CubicHermiteCurve2D::CubicHermiteCurve2D(const CubicBezierCurve2D& fromBezier)
{
    m_velocityU = 3 * (fromBezier.m_guidePos1 - fromBezier.m_startPos);
    m_velocityV = 3 * (fromBezier.m_endPos - fromBezier.m_guidePos2);
}

Vec2 CubicHermiteCurve2D::EvaluateAtParametric(float parametricZeroToOne) const
{
    auto curve = CubicBezierCurve2D(*this);
    return curve.EvaluateAtParametric(parametricZeroToOne);
}

Vec2 CubicHermiteCurve2D::EvaluateAtApproximateDistance(float distanceAlongCurve, int numSubdivisions) const
{
    auto curve = CubicBezierCurve2D(*this);
    return curve.EvaluateAtApproximateDistance(distanceAlongCurve, numSubdivisions);
}

float CubicHermiteCurve2D::GetLength(int numSubdivisions) const
{
    auto curve = CubicBezierCurve2D(*this);
    return curve.GetLength(numSubdivisions);
}
