#include "RaycastUtils.hpp"
#include "AABB2.hpp"
#include "FloatRange.hpp"
#include "LineSegment2.hpp"
#include "MathUtils.hpp"
#include "ZCylinder.hpp"
#include "Engine/Core/EngineCommon.hpp"

RaycastResult2D RaycastVsDisc2D(Vec2 startPos, Vec2 fwdNormal, float maxDist, Vec2 discCenter, float discRadius)
{
    RaycastResult2D result;
    Vec2            SC = discCenter - startPos;
    Vec2&           i  = fwdNormal;
    Vec2            j  = i.GetRotated90Degrees();

    // Project SC onto i (forward direction) and j (perpendicular direction)
    float SCj = DotProduct2D(SC, j);

    // We want the point in side disc return the negative fwdNormal
    if (IsPointInsideDisc2D(startPos, discCenter, discRadius))
    {
        result.m_didImpact    = true;
        result.m_impactNormal = -fwdNormal;
        result.m_impactPos    = startPos;
        return result;
    }

    // If the perpendicular distance is greater than the disc radius, no impact occurs
    if (SCj > discRadius || SCj < -discRadius)
    {
        result.m_didImpact = false;
        return result;
    }

    // Project SC onto i to find the distance along the ray direction
    float SCi = DotProduct2D(SC, i);

    // If the projection along the ray is beyond maxDist or behind the start position, no impact
    if (SCi < -discRadius || SCi > maxDist + discRadius)
    {
        result.m_didImpact = false;
        return result;
    }

    // Calculate the closest approach along the ray direction
    float closestApproach = sqrtf((discRadius * discRadius) - (SCj * SCj));
    float impactDist      = SCi - closestApproach;

    // If the impact distance is negative or beyond maxDist, no impact occurs
    if (impactDist < 0.f || impactDist > maxDist)
    {
        result.m_didImpact = false;
        return result;
    }


    // Impact occurs, populate result
    result.m_didImpact    = true;
    result.m_impactDist   = impactDist;
    result.m_impactPos    = startPos + (fwdNormal * impactDist);
    result.m_impactNormal = (result.m_impactPos - discCenter).GetNormalized();


    // Store original ray information in the result
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayStartPos  = startPos;
    result.m_rayMaxLength = maxDist;

    return result;
}

RaycastResult2D RaycastVsLineSegment2D(Vec2 startPos, Vec2 fwdNormal, float maxDist, LineSegment2& lineSegment2D)
{
    RaycastResult2D result;

    Vec2  j                = fwdNormal.GetRotated90Degrees();
    Vec2  endPos           = startPos + fwdNormal * maxDist;
    Vec2  startToLineStart = lineSegment2D.m_start - startPos; // RS
    Vec2  startToLineEnd   = lineSegment2D.m_end - startPos; // RE
    float RSj              = DotProduct2D(startToLineStart, j);
    float REj              = DotProduct2D(startToLineEnd, j);
    if (RSj * REj >= 0)
    {
        result.m_didImpact = false;
        return result;
    }
    float RSi = DotProduct2D(startToLineStart, fwdNormal);
    float REi = DotProduct2D(startToLineEnd, fwdNormal);
    if (RSi > maxDist && REi >= maxDist)
    {
        result.m_didImpact = false;
        return result;
    }
    if (RSi <= 0 && REi <= 0)
    {
        result.m_didImpact = false;
        return result;
    }
    float tImpact        = RSj / (RSj + (-REj));
    float impactDistance = RSi + tImpact * (REi - RSi);
    if (impactDistance < 0 || impactDistance >= maxDist)
    {
        result.m_didImpact = false;
        return result;
    }
    Vec2 impactPos        = startPos + (fwdNormal * impactDistance);
    result.m_impactNormal = (lineSegment2D.m_end - lineSegment2D.m_start).GetRotated90Degrees().GetNormalized();
    if (DotProduct2D(fwdNormal, result.m_impactNormal) > 0)
    {
        result.m_impactNormal = -result.m_impactNormal;
    }
    result.m_impactPos  = impactPos;
    result.m_didImpact  = true;
    result.m_impactDist = impactDistance;

    // Store original ray information in the result
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayStartPos  = startPos;
    result.m_rayMaxLength = maxDist;
    return result;
}

RaycastResult2D RaycastVsAABB2(Vec2 startPos, Vec2 fwdNormal, float maxDist, AABB2& aabb2)
{
    RaycastResult2D result;
    result.m_didImpact    = false;
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayStartPos  = startPos;
    result.m_rayMaxLength = maxDist;

    if (aabb2.IsPointInside(startPos))
    {
        result.m_didImpact    = true;
        result.m_impactDist   = 0.f;
        result.m_impactPos    = startPos;
        result.m_impactNormal = -fwdNormal;
        return result;
    }

    FloatRange tRangeX;
    {
        if (fabsf(fwdNormal.x) > 0.f)
        {
            float invFx = 1.f / fwdNormal.x;
            float t1    = (aabb2.m_mins.x - startPos.x) * invFx;
            float t2    = (aabb2.m_maxs.x - startPos.x) * invFx;
            tRangeX     = FloatRange(std::min(t1, t2), std::max(t1, t2));
        }
        else
        {
            FloatRange boxXRange(aabb2.m_mins.x, aabb2.m_maxs.x);
            if (!boxXRange.IsOnRange(startPos.x))
            {
                return result;
            }
            tRangeX = FloatRange(-FLT_MAX, FLT_MAX);
        }
    }

    FloatRange tRangeY;
    {
        if (fabsf(fwdNormal.y) > 0.f)
        {
            float invFy = 1.f / fwdNormal.y;
            float t1    = (aabb2.m_mins.y - startPos.y) * invFy;
            float t2    = (aabb2.m_maxs.y - startPos.y) * invFy;
            tRangeY     = FloatRange(std::min(t1, t2), std::max(t1, t2));
        }
        else
        {
            FloatRange boxYRange(aabb2.m_mins.y, aabb2.m_maxs.y);
            if (!boxYRange.IsOnRange(startPos.y))
            {
                return result;
            }
            tRangeY = FloatRange(-FLT_MAX, FLT_MAX);
        }
    }

    if (!tRangeX.IsOverlappingWith(tRangeY))
    {
        return result;
    }

    FloatRange overlapXY(
        std::max(tRangeX.m_min, tRangeY.m_min),
        std::min(tRangeX.m_max, tRangeY.m_max)
    );

    if (overlapXY.m_max < 0.f || overlapXY.m_min > overlapXY.m_max)
    {
        return result;
    }

    FloatRange validRange(0.f, maxDist);
    if (!overlapXY.IsOverlappingWith(validRange))
    {
        return result;
    }

    FloatRange finalRange(
        std::max(overlapXY.m_min, validRange.m_min),
        std::min(overlapXY.m_max, validRange.m_max)
    );

    if (finalRange.m_min > finalRange.m_max)
    {
        return result;
    }

    float tEnter = finalRange.m_min;

    result.m_didImpact  = true;
    result.m_impactDist = tEnter;
    result.m_impactPos  = startPos + fwdNormal * tEnter;

    float tMinX = tRangeX.m_min;
    float tMinY = tRangeY.m_min;

    if (tMinX > tMinY)
    {
        if (fwdNormal.x > 0.f)
            result.m_impactNormal = Vec2(-1.f, 0.f);
        else
            result.m_impactNormal = Vec2(1.f, 0.f);
    }
    else
    {
        if (fwdNormal.y > 0.f)
            result.m_impactNormal = Vec2(0.f, -1.f);
        else
            result.m_impactNormal = Vec2(0.f, 1.f);
    }

    return result;
}

RaycastResult3D RaycastVsSphere3D(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const Sphere& sphere)
{
    RaycastResult3D result;
    result.m_didImpact    = false;
    result.m_rayStartPos  = startPos;
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayMaxLength = maxDist;

    float distSqToCenter = GetDistanceSquared3D(startPos, sphere.m_position);
    float radiusSq       = sphere.m_radius * sphere.m_radius;
    if (distSqToCenter <= radiusSq)
    {
        result.m_didImpact    = true;
        result.m_impactDist   = 0.f;
        result.m_impactPos    = startPos;
        result.m_impactNormal = -fwdNormal;
        return result;
    }

    Vec3  SC  = sphere.m_position - startPos;
    float SCi = DotProduct3D(SC, fwdNormal);

    if (SCi < -sphere.m_radius || SCi > maxDist + sphere.m_radius)
    {
        return result;
    }

    float SC_lenSq   = DotProduct3D(SC, SC);
    float perpDistSq = SC_lenSq - (SCi * SCi);
    if (perpDistSq > radiusSq)
    {
        return result;
    }

    float offset     = sqrtf(radiusSq - perpDistSq);
    float impactDist = SCi - offset;

    if (impactDist < 0.f || impactDist > maxDist)
    {
        return result;
    }

    result.m_didImpact    = true;
    result.m_impactDist   = impactDist;
    result.m_impactPos    = startPos + (fwdNormal * impactDist);
    result.m_impactNormal = (result.m_impactPos - sphere.m_position).GetNormalized();

    return result;
}

RaycastResult3D RaycastVsAABB3D(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const AABB3& aabb3)
{
    RaycastResult3D result;
    result.m_didImpact    = false;
    result.m_rayStartPos  = startPos;
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayMaxLength = maxDist;

    if (aabb3.IsPointInside(startPos))
    {
        result.m_didImpact    = true;
        result.m_impactDist   = 0.f;
        result.m_impactPos    = startPos;
        result.m_impactNormal = -fwdNormal;
        return result;
    }

    float tMin = 0.f;
    float tMax = maxDist;

    if (fwdNormal.x != 0.f)
    {
        float invFX = 1.f / fwdNormal.x;
        float t1    = (aabb3.m_mins.x - startPos.x) * invFX;
        float t2    = (aabb3.m_maxs.x - startPos.x) * invFX;
        if (t1 > t2)
        {
            float temp = t1;
            t1         = t2;
            t2         = temp;
        }
        if (t2 < tMin || t1 > tMax)
        {
            return result;
        }
        if (t1 > tMin) { tMin = t1; }
        if (t2 < tMax) { tMax = t2; }
        if (tMin > tMax)
        {
            return result;
        }
    }
    else
    {
        if (startPos.x < aabb3.m_mins.x || startPos.x > aabb3.m_maxs.x)
        {
            return result;
        }
    }

    if (fwdNormal.y != 0.f)
    {
        float invFY = 1.f / fwdNormal.y;

        float t1 = (aabb3.m_mins.y - startPos.y) * invFY;
        float t2 = (aabb3.m_maxs.y - startPos.y) * invFY;
        if (t1 > t2)
        {
            float temp = t1;
            t1         = t2;
            t2         = temp;
        }
        if (t2 < tMin || t1 > tMax)
        {
            return result;
        }
        if (t1 > tMin) { tMin = t1; }
        if (t2 < tMax) { tMax = t2; }
        if (tMin > tMax)
        {
            return result;
        }
    }
    else
    {
        if (startPos.y < aabb3.m_mins.y || startPos.y > aabb3.m_maxs.y)
        {
            return result;
        }
    }

    if (fwdNormal.z != 0.f)
    {
        float invFZ = 1.f / fwdNormal.z;

        float t1 = (aabb3.m_mins.z - startPos.z) * invFZ;
        float t2 = (aabb3.m_maxs.z - startPos.z) * invFZ;
        if (t1 > t2)
        {
            float temp = t1;
            t1         = t2;
            t2         = temp;
        }
        if (t2 < tMin || t1 > tMax)
        {
            return result;
        }
        if (t1 > tMin) { tMin = t1; }
        if (t2 < tMax) { tMax = t2; }
        if (tMin > tMax)
        {
            return result;
        }
    }
    else
    {
        if (startPos.z < aabb3.m_mins.z || startPos.z > aabb3.m_maxs.z)
        {
            return result;
        }
    }

    if (tMin < 0.f || tMin > maxDist)
    {
        return result;
    }

    result.m_didImpact  = true;
    result.m_impactDist = tMin;
    result.m_impactPos  = startPos + (fwdNormal * tMin);

    constexpr float EPSILON = 1.e-5f;

    if (fwdNormal.x != 0.f)
    {
        float testT1 = ((fwdNormal.x > 0.f) ? (aabb3.m_mins.x - startPos.x) : (aabb3.m_maxs.x - startPos.x)) * (1.f / fwdNormal.x);

        if (fabsf(testT1 - tMin) < EPSILON)
        {
            // 命中X面
            if (fwdNormal.x > 0.f)
            {
                result.m_impactNormal = Vec3(-1.f, 0.f, 0.f);
            }
            else
            {
                result.m_impactNormal = Vec3(1.f, 0.f, 0.f);
            }
            return result;
        }
    }

    if (fwdNormal.y != 0.f)
    {
        float testT1 = ((fwdNormal.y > 0.f) ? (aabb3.m_mins.y - startPos.y) : (aabb3.m_maxs.y - startPos.y)) * (1.f / fwdNormal.y);

        if (fabsf(testT1 - tMin) < EPSILON)
        {
            if (fwdNormal.y > 0.f)
            {
                result.m_impactNormal = Vec3(0.f, -1.f, 0.f);
            }
            else
            {
                result.m_impactNormal = Vec3(0.f, 1.f, 0.f);
            }
            return result;
        }
    }

    if (fwdNormal.z != 0.f)
    {
        float testT1 = ((fwdNormal.z > 0.f) ? (aabb3.m_mins.z - startPos.z) : (aabb3.m_maxs.z - startPos.z)) * (1.f / fwdNormal.z);

        if (fabsf(testT1 - tMin) < EPSILON)
        {
            if (fwdNormal.z > 0.f)
            {
                result.m_impactNormal = Vec3(0.f, 0.f, -1.f);
            }
            else
            {
                result.m_impactNormal = Vec3(0.f, 0.f, 1.f);
            }
            return result;
        }
    }

    result.m_impactNormal = -fwdNormal;
    return result;
}


RaycastResult3D RaycastVsZCylinder3D(const Vec3 startPos, const Vec3 fwdNormal, float maxDist, const ZCylinder& cylinder)
{
    RaycastResult3D result;
    result.m_didImpact    = false;
    result.m_rayStartPos  = startPos;
    result.m_rayFwdNormal = fwdNormal;
    result.m_rayMaxLength = maxDist;

    float halfHeight = cylinder.m_height * 0.5f;
    float zMin       = cylinder.m_center.z - halfHeight;
    float zMax       = cylinder.m_center.z + halfHeight;

    {
        if (IsPointInsideZCylinder3D(startPos, cylinder))
        {
            result.m_didImpact    = true;
            result.m_impactDist   = 0.f;
            result.m_impactPos    = startPos;
            result.m_impactNormal = -fwdNormal;
            return result;
        }
    }

    float tSide = -1.f;
    Vec3  sidePos;
    bool  sideHit = false;

    {
        float cx = cylinder.m_center.x;
        float cy = cylinder.m_center.y;
        float r  = cylinder.m_radius;

        float px = startPos.x;
        float py = startPos.y;
        float dx = fwdNormal.x;
        float dy = fwdNormal.y;

        float EPSILON = 1.e-20f;
        float dd      = (dx * dx + dy * dy);
        // I use another way (math equation to solve this parts)
        if (dd > EPSILON)
        {
            float A = dx * dx + dy * dy;
            float B = 2.f * (dx * (px - cx) + dy * (py - cy));
            float C = (px - cx) * (px - cx) + (py - cy) * (py - cy) - r * r;

            float discriminant = B * B - 4.f * A * C;
            if (discriminant >= 0.f)
            {
                float sqrtD = sqrtf(discriminant);
                float inv2A = 1.f / (2.f * A);

                float t1 = (-B - sqrtD) * inv2A;
                float t2 = (-B + sqrtD) * inv2A;

                float nearT = (t1 < t2) ? t1 : t2;
                float farT  = (t1 < t2) ? t2 : t1;

                float candidateT = -1.f;
                if (nearT >= 0.f) candidateT = nearT;
                else if (farT >= 0.f) candidateT = farT;
                if (candidateT >= 0.f && candidateT <= maxDist)
                {
                    Vec3  hitPos = startPos + (candidateT * fwdNormal);
                    float zVal   = hitPos.z;
                    if (zVal >= zMin && zVal <= zMax)
                    {
                        sideHit = true;
                        tSide   = candidateT;
                        sidePos = hitPos;
                    }
                }
            }
        }
    }

    bool  capHit = false;
    float tCap   = -1.f;
    Vec3  capPos;
    // lambda to solve the z plane projection
    auto IntersectPlaneZ = [&](float planeZ)
    {
        float fz = fwdNormal.z;
        float sz = startPos.z;

        if (fabsf(fz) < 1.e-20f) return -1.f;

        float t = (planeZ - sz) / fz;
        if (t < 0.f || t > maxDist)
        {
            return -1.f;
        }

        Vec3 p = startPos + t * fwdNormal;

        float dx = p.x - cylinder.m_center.x;
        float dy = p.y - cylinder.m_center.y;
        float rr = dx * dx + dy * dy;
        if (rr > cylinder.m_radius * cylinder.m_radius)
        {
            return -1.f;
        }
        return t;
    };

    float tBottom = IntersectPlaneZ(zMin);
    float tTop    = IntersectPlaneZ(zMax);

    float tPlane = -1.f;

    if (tBottom >= 0.f && tTop >= 0.f)
    {
        if (tBottom < tTop)
        {
            tPlane = tBottom;
        }
        else
        {
            tPlane = tTop;
        }
    }
    else if (tBottom >= 0.f)
    {
        tPlane = tBottom;
    }
    else if (tTop >= 0.f)
    {
        tPlane = tTop;
    }

    if (tPlane >= 0.f)
    {
        capHit = true;
        tCap   = tPlane;
        capPos = startPos + tPlane * fwdNormal;
    }

    if (!sideHit && !capHit)
    {
        return result;
    }

    float finalT = -1.f;
    Vec3  finalPos;
    Vec3  finalNorm;

    if (sideHit && !capHit)
    {
        finalT   = tSide;
        finalPos = sidePos;
        Vec2 dirXY(finalPos.x - cylinder.m_center.x, finalPos.y - cylinder.m_center.y);
        dirXY.Normalize();
        finalNorm = Vec3(dirXY.x, dirXY.y, 0.f);
    }
    else if (!sideHit && capHit)
    {
        finalT       = tCap;
        finalPos     = capPos;
        float planeZ = (fabsf(capPos.z - zMin) < 0.0001f) ? zMin : zMax;
        if (planeZ == zMin)
        {
            finalNorm = Vec3(0, 0, -1);
        }
        else
        {
            finalNorm = Vec3(0, 0, 1);
        }
    }
    else
    {
        if (tSide < tCap)
        {
            finalT   = tSide;
            finalPos = sidePos;
            Vec2 dirXY(finalPos.x - cylinder.m_center.x, finalPos.y - cylinder.m_center.y);
            dirXY.Normalize();
            finalNorm = Vec3(dirXY.x, dirXY.y, 0.f);
        }
        else
        {
            finalT       = tCap;
            finalPos     = capPos;
            float planeZ = (fabsf(capPos.z - zMin) < 0.0001f) ? zMin : zMax;
            if (planeZ == zMin)
            {
                finalNorm = Vec3(0, 0, -1);
            }
            else
            {
                finalNorm = Vec3(0, 0, 1);
            }
        }
    }

    result.m_didImpact    = true;
    result.m_impactDist   = finalT;
    result.m_impactPos    = finalPos;
    result.m_impactNormal = finalNorm;

    return result;
}
