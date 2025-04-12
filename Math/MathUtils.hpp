#pragma once
#include "AABB3.hpp"
#include "Disc2.hpp"
#include "Mat44.hpp"
#include "Vec2.hpp"
#include "Vec4.hpp"
// Standalone Functions File

// Forward type declarations

class ZCylinder;
struct Sphere;
class AABB2;
class OBB2;
struct LineSegment2;
struct Capsule2;
struct Triangle2;
struct Disc2;

struct IntVec2;
struct Vec2;
struct Vec3;

enum class BillboardType
{
    NONE = -1,
    WORLD_UP_FACING,
    WORLD_UP_OPPOSING,
    FULL_FACING,
    FULL_OPPOSING,
    COUNT
};


// Clamp and lerp
float GetClamped(float value, float minValue, float maxValue);
float GetClampedZeroToOne(float value);
float Interpolate(float start, float end, float fractionTowardEnd);
Vec2  Interpolate(Vec2 start, Vec2 end, float fractionTowardEnd);
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
float DotProduct3D(const Vec3& a, const Vec3& b);
float DotProduct4D(const Vec4& a, const Vec4& b);
float CrossProduct2D(const Vec2& a, const Vec2& b);
Vec3  CrossProduct3D(const Vec3& a, const Vec3& b);

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
bool DoSpheresOverlap(const Sphere& sphereA, const Sphere& sphereB);
bool DoAABB3DOverlap(const AABB3& aabbA, const AABB3& aabbB);
bool DoZCylinder3DOverlap(const ZCylinder& cylinderA, const ZCylinder& cylinderB);
bool DoSphereAndAABB3DOverlap(const Sphere& sphere, const AABB3& aabb);
bool DoZCylinder3DAndAABB3DOverlap(const AABB3& aabb, const ZCylinder& cylinder);
bool DoZCylinder3DAndSphereOverlap(const Sphere& sphere, const ZCylinder& cylinder);
// Get Nearest Point series
Vec2 GetNearestPointOnDisc2D(const Vec2& referencePosition, const Vec2& discCenter, float discRadius);
Vec2 GetNearestPointOnDisc2D(const Vec2& referencePosition, const Disc2& disc);
Vec2 GetNearestPointOnAABB2D(const Vec2& referencePos, const AABB2& alignedBox);
Vec2 GetNearestPointOnOBB2D(const Vec2& referencePos, const OBB2& alignedBox);
Vec2 GetNearestPointOnInfiniteLine2D(const Vec2& referencePos, const LineSegment2& infiniteLine);
Vec2 GetNearestPointOnLineSegment2D(const Vec2& referencePos, const LineSegment2& lineSegment);
Vec2 GetNearestPointOnCapsule2D(const Vec2& referencePos, const Capsule2& capsule);
Vec2 GetNearestPointOnTriangle2D(const Vec2& referencePos, const Triangle2& triangle);
Vec3 GetNearestPointOnSphere(const Vec3& referencePosition, const Sphere& sphere);
Vec3 GetNearestPointOnCube3D(const Vec3& referencePos, const AABB3& aabb);
Vec3 GetNearestPointOnZCylinder3D(const Vec3& referencePos, const ZCylinder& cylinder);

// Is point Inside series
bool IsPointInsideDisc2D(const Vec2& point, const Vec2& discCenter, float discRadius);
bool IsPointInsideDisc2D(const Vec2& point, const Disc2& disc);
bool IsPointInsideAABB2D(const Vec2& point, const AABB2& box);
bool IsPointInsideOBB2D(const Vec2& point, const OBB2& box);
bool IsPointInsideZCylinder3D(const Vec3& point, const ZCylinder& cylinder);
bool IsPointInsideCapsule(const Vec2& point, const Capsule2& capsule);
bool IsPointInsideTriangle(const Vec2& point, const Triangle2& triangle);
bool IsPointInsideOrientedSector2D(const Vec2& point, const Vec2& sectorTip, float sectorForwardDegrees, float sectorApertureDegrees, float sectorRadius);
bool IsPointInsideDirectedSector2D(const Vec2& point, const Vec2& sectorTip, const Vec2& sectorForwardNormal, float sectorApertureDegrees, float sectorRadius);

/// Disc Push
bool PushDiscOutOfPoint2D(Vec2& mobileDiscCenter, float discRadius, const Vec2& fixedPoint); // MP1A4
bool PushDiscOutOfPoint2D(Disc2& mobileDisc, const Vec2& fixedPoint);
bool PushDiscOutOfCapsule2D(Vec2& mobileDiscCenter, float discRadius, const Capsule2& fixedCapsule2D);
bool PushDiscOutOfOBB2D(Vec2& mobileDiscCenter, float discRadius, const OBB2& fixedOBB2D);
bool PushDiscOutOfDisc2D(Vec2& mobileDiscCenter, float discRadius, const Vec2& fixedDiscCenter, float fixedDiscRadius); // MP1A4
bool PushDiscsOutOfEachOther2D(Vec2& aCenter, float aRadius, Vec2& bCenter, float bRadius);
bool PushDiscsOutOfEachOther2D(Disc2& mobileDiscA, Disc2& mobileDiscB);
bool PushDiscOutOfAABB2D(Vec2& mobileDiscCenter, float discRadius, const AABB2& fixedBox); // MP1A4

/// Bounce

bool BounceDiscOffPoint2D(Disc2& mobileDisc, Vec2& mobileDiscVelocity, float mobileDiscElasticity, Vec2& bounceOffPoint /* Fixed Point*/);
bool BounceDiscsOffEachOther(Disc2& mobileDiscA, Vec2& mobileDiscVelocityA, float mobileDiscElasticityA, Disc2& mobileDiscB, Vec2& mobileDiscVelocityB, float mobileDiscElasticityB);
bool BounceDiscsOffEachOther(Vec2& mobileDiscCenterA, float   mobileDiscRadiusA, Vec2& mobileDiscVelocityA, float mobileDiscElasticityA, Vec2& mobileDiscCenterB, float mobileDiscRadiusB,
                             Vec2& mobileDiscVelocityB, float mobileDiscElasticityB);
bool BounceDiscOffCapsule2D(Vec2& mobileDiscCenter, float mobileDiscRadiusA, Vec2& mobileDiscVelocity, float mobileDiscElasticity, const Capsule2& fixedCapsule2D, float capsule2DElasticity);
bool BounceDiscOffDisc2D(Vec2& mobileDiscCenter, float mobileDiscRadiusA, Vec2& mobileDiscVelocity, float mobileDiscElasticity, const Disc2& fixedDisc2D, float disc2DElasticity);
bool BounceDiscOffOBB2D(Vec2& mobileDiscCenter, float mobileDiscRadius, Vec2& mobileDiscVelocity, float mobileDiscElasticity, const OBB2& fixedOBB2D, float OBB2DElasticity);
// Transform utilities
void TransformPosition2D(Vec2& posToTransform, float uniformScale, float rotationDegrees, const Vec2& translation);
void TransformPositionXY3D(Vec3& positionToTransform, float scaleXY, float zRotationDegrees, const Vec2& translationXY);
void TransformPosition2D(Vec2& posToTransform, const Vec2& iBasis, const Vec2& jBasis, const Vec2& translation);
void TransformPositionXY3D(Vec3& posToTransform, const Vec2& iBasis, const Vec2& jBasis, const Vec2& translation);

// Expression
int RecursiveSum(int From);

// Other
float         NormalizeByte(unsigned char byte);
unsigned char DenormalizeByte(float range);

// Billboard
Mat44 GetBillboardTransform(BillboardType billboardType, Mat44 const& targetTransform, Vec3 const& billboardPosition, Vec2 const& billboardScale = Vec2(1.0f, 1.0f));

/// Periodical
float CycleValue(float time, float period);
/// 
