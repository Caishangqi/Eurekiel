#pragma once
#include "Engine/Math/Vec2.hpp"

struct Vec4;

enum class AABB2Anchor
{
    INVALID = -1,
    BOTTOM_LEFT,
    BOTTOM_RIGHT,
    TOP_LEFT,
    TOP_RIGHT
};

class AABB2
{
public:
    Vec2 m_mins; // left conner
    Vec2 m_maxs; // right up

    static AABB2 ZERO_TO_ONE;

    // Construction / Destruction
    ~AABB2();
    AABB2();
    AABB2(const AABB2& copyFrom);
    explicit AABB2(float minX, float minY, float maxX, float maxY); // explicit constructor (from x1, y1, x2, y2)
    explicit AABB2(const Vec2& mins, const Vec2& maxs); // explicit constructor (from mins, maxs)

    // Accessors (const methods)
    bool       IsPointInside(const Vec2& point) const;
    const Vec2 GetCenter() const;
    const Vec2 GetDimensions() const;
    const Vec2 GetNearestPoint(const Vec2& referencePosition) const;
    const Vec2 GetPointAtUV(const Vec2& uv) const; // uv = (0,0) is at mins; uv = (1,1) is at maxs
    const Vec2 GetUVForPoint(const Vec2& point) const; // uv = (.5,.5) at center; u or v outside [0,1] extrapolated

    /// Helpers
    AABB2 GetBoxAtUvs(Vec2 uvMin, Vec2 uvMax);
    void  AddPadding(float xToAddOnBothSides, float yToAddOnBothSides);
    void  ReduceToAspect(float newAspectRatio); /// reduces to new aspect ratio, keeping center the same
    void  EnlargeToAspect(float newAspect); /// enlarges to new aspect ratio, keeping center the same
    AABB2 StretchTowards(AABB2Anchor anchor, Vec2 stretchAmount);
    AABB2 GetPadded(const Vec4& padding) const;

    /// Chop off the top AABB2 and return the chopped topped aabb2
    AABB2 ChopOffTop(float heightOfChoppedPieces);

    // Mutators (non-const methods)
    void Translate(const Vec2& translationToApply);
    void SetCenter(const Vec2& newCenter);
    void SetDimensions(const Vec2& newDimensions);
    void StretchToIncludePoint(const Vec2& point); // does minimal stretching required (none if already on point)

    friend bool operator==(const AABB2& lhs, const AABB2& rhs);
    friend bool operator!=(const AABB2& lhs, const AABB2& rhs);
};
