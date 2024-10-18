#pragma once
// Standalone Functions File

// Forward type declarations

class AABB2;
struct IntVec2;
struct Vec2;
struct Vec3;

// Clamp and lerp
float GetClamped(float value, float minValue, float maxValue);
float GetClampedZeroToOne(float value);
float Interpolate(float start, float end, float fractionTowardEnd);
float GetFractionWithinRange(float value, float rangeStart, float rangeEnd);
float RangeMap(float inValue, float inStart, float inEnd, float outStart, float outEnd);
float RangeMapClamped(float inValue, float inStart, float inEnd, float outStart, float outEnd);
int   RoundDownToInt(float value);

// Angle utilities
float ConvertDegreesToRadians(float degrees);
float ConvertRadiansToDegrees(float radians);
float CosDegrees(float degrees);
float SinDegrees(float degrees);
float Atan2Degrees(float y, float x);
float GetShortestAngularDispDegrees(float startDegree, float endDegree);
float GetTurnedTowardDegrees(float currentDegrees, float goalDegrees, float maxDeltaDegrees);
float GetAngleDegreesBetweenVectors2D(const Vec2& first, const Vec2& second);

// Dot and Cross
float DotProduct2D(const Vec2& a, const Vec2& b);

// Basic 2D and 3D utilities
float GetDistance2D(const Vec2& positionA, const Vec2& positionB);
float GetDistanceSquared2D(const Vec2& positionA, const Vec2& positionB);
float GetDistance3D(const Vec3& positionA, const Vec3& positionB);
float GetDistanceSquared3D(const Vec3& positionA, const Vec3& positionB);
float GetDistanceXY3D(const Vec3& positionA, const Vec3& positionB);
float GetDistanceXYSquared3D(const Vec3& positionA, const Vec3& positionB);
int   GetTaxicabDistance2D(const IntVec2& pointA, const IntVec2& pointB);
float GetProjectedLength2D(const Vec2& vectorToProject, const Vec2& vectorToProjectOnto);
// Works if Vecs not normalized
const Vec2 GetProjectedOnto2D(const Vec2& vectorToProject, const Vec2& vectorToProjectOnto);
// Works if Vecs not normalized

// Geometric query utilities
bool DoDiscsOverlap(const Vec2& centerA, float radiusA, const Vec2& centerB, float radiusB);
bool DoSpheresOverlap(const Vec3& centerA, float radiusA, const Vec3& centerB, float radiusB);
Vec2 GetNearestPointOnDisc2D(const Vec2& referencePosition, const Vec2& discCenter, float discRadius);
bool IsPointInsideDisc2D(const Vec2& point, const Vec2& discCenter, float discRadius);
bool IsPointInsideOrientedSector2D(const Vec2& point, const Vec2&           sectorTip, float sectorForwardDegrees,
                                   float       sectorApertureDegrees, float sectorRadius);
bool IsPointInsideDirectedSector2D(const Vec2& point, const Vec2&           sectorTip, const Vec2& sectorForwardNormal,
                                   float       sectorApertureDegrees, float sectorRadius);
bool PushDiscOutOfPoint2D(Vec2& mobileDiscCenter, float discRadius, const Vec2& fixedPoint); // MP1A4

bool PushDiscOutOfDisc2D(Vec2& mobileDiscCenter, float discRadius, const Vec2& fixedDiscCenter,
                         float fixedDiscRadius); // MP1A4
bool PushDiscsOutOfEachOther2D(Vec2& aCenter, float aRadius, Vec2& bCenter, float bRadius);

bool PushDiscOutOfAABB2D(Vec2& mobileDiscCenter, float discRadius, const AABB2& fixedBox); // MP1A4


// Transform utilities
void TransformPosition2D(Vec2& posToTransform, float uniformScale, float rotationDegrees, const Vec2& translation);
void TransformPositionXY3D(Vec3& positionToTransform, float scaleXY, float zRotationDegrees, const Vec2& translationXY);
void TransformPosition2D(Vec2& posToTransform, const Vec2& iBasis, const Vec2& jBasis, const Vec2& translation);
void TransformPositionXY3D(Vec3& posToTransform, const Vec2& iBasis, const Vec2& jBasis, const Vec2& translation);

// Expression
int RecursiveSum (int From);
