#include "CubicHermiteSpline.hpp"

#include "Engine/Math/MathUtils.hpp"

CubicHermiteSpline::CubicHermiteSpline(std::vector<Vec2> points): m_points(points)
{
    if (points.size() < 2)return;

    int               N = static_cast<int>(points.size());
    std::vector<Vec2> v(N, Vec2::ZERO);

    v[0]     = Vec2::ZERO;
    v[N - 1] = Vec2::ZERO;

    for (int i = 1; i < N - 1; i++)
    {
        v[i] = 0.5f * (points[i + 1] - points[i - 1]);
    }

    m_curves.reserve(N - 1);
    for (int i = 0; i < N - 1; i++)
    {
        Vec2 startPos = points[i];
        Vec2 endPos   = points[i + 1];

        Vec2 startVel = v[i];
        Vec2 endVel   = v[i + 1];

        m_curves.push_back(CubicHermiteCurve2D(startPos, startVel, endVel, endPos));
    }
}

std::vector<Vec2>& CubicHermiteSpline::GetPoints()
{
    return m_points;
}

Vec2 CubicHermiteSpline::EvaluateAtParametric(float parametricZeroToNumCurvesSections) const
{
    if (parametricZeroToNumCurvesSections >= static_cast<float>(m_curves.size())) return m_curves.back().EvaluateAtParametric(1);
    return m_curves[static_cast<int>(parametricZeroToNumCurvesSections)].EvaluateAtParametric(parametricZeroToNumCurvesSections - floorf(parametricZeroToNumCurvesSections));
}

Vec2 CubicHermiteSpline::EvaluateAtApproximateDistance(float distanceAlongCurve, int numSubdivisions) const
{
    float totalLength = GetLength(numSubdivisions);
    if (distanceAlongCurve <= 0.f) return m_curves.front().m_startPos;
    if (distanceAlongCurve >= totalLength) return m_curves.back().m_endPos;
    Vec2  prevPos  = EvaluateAtParametric(0.f);
    float traveled = 0.f;
    float step     = 1.f / static_cast<float>(numSubdivisions);

    for (int i = 1; i <= numSubdivisions * static_cast<int>(m_curves.size()); ++i)
    {
        float t             = step * static_cast<float>(i);
        Vec2  currPos       = EvaluateAtParametric(t);
        float segmentLength = (currPos - prevPos).GetLength();

        // Check if the target distance falls within this segment
        if (distanceAlongCurve <= traveled + segmentLength)
        {
            float fraction = (distanceAlongCurve - traveled) / segmentLength;
            return Interpolate(prevPos, currPos, fraction);
        }

        traveled += segmentLength;
        prevPos = currPos;
    }

    return m_curves.back().m_endPos;
}

float CubicHermiteSpline::GetLength(int numSubdivisions) const
{
    float length = 0.f;
    for (const CubicHermiteCurve2D& curve : m_curves)
    {
        length += curve.GetLength(numSubdivisions);
    }
    return length;
}

int CubicHermiteSpline::GetNumOfCurves() const
{
    return static_cast<int>(m_curves.size());
}

std::vector<CubicHermiteCurve2D>& CubicHermiteSpline::GetCurves()
{
    return m_curves;
}
