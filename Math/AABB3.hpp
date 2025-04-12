#pragma once
#include "Vec3.hpp"

class AABB3
{
public:
    Vec3 m_mins;
    Vec3 m_maxs;

    ~AABB3();

    AABB3()
    {
    }

    // Accessors (const methods)
    bool       IsPointInside(const Vec3& point) const;
    const Vec3 GetCenter() const;
    const Vec3 GetDimensions() const;

    // Mutators (non-const methods)
    void Translate(const Vec3& translationToApply);
    void SetCenter(const Vec3& newCenter);
    void SetDimensions(const Vec3& newDimensions);
    void StretchToIncludePoint(const Vec3& point); // does minimal stretching required (none if already on point)
    
    AABB3(const AABB3& copyFrom);
    explicit AABB3(float minX, float minY, float minZ, float maxX, float maxY, float maxZ);
    explicit AABB3(const Vec3& mins, const Vec3& maxs);
};
