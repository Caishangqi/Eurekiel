#pragma once
#include <vector>

#include "Rgba8.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec3.hpp"

class Plane3;
class OBB3;
struct Vertex_PCUTBN;
class ZCylinder;
struct Mat44;
struct Vec3;
struct Rgba8;
struct Triangle2;
struct Capsule2;
struct LineSegment2;
class OBB2;
class AABB2;
struct Disc2;
struct Vec2;
struct Vertex_PCU;

void TransformVertexArrayXY3D(int numVerts, Vertex_PCU* verts, float uniformScaleXY, float rotationDegreesAboutZ, const Vec2& translationXY);
void TransformVertexArrayXY3D(std::vector<Vertex_PCU>& verts, float uniformScaleXY, float rotationDegreesAboutZ, const Vec2& translationXY);
void AddVertsForDisc2D(std::vector<Vertex_PCU>& verts, const Disc2& disc, const Rgba8& color);
void AddVertsForAABB2D(std::vector<Vertex_PCU>& verts, const AABB2& aabb2, const Rgba8& color);
void AddVertsForCurve2D(std::vector<Vertex_PCU>& verts, const std::vector<Vec2>& points, const Rgba8& color, float thickness = 1.0f);
void AddVertsForAABB2D(std::vector<Vertex_PCU>& verts, const AABB2& aabb2, const Rgba8& color, const Vec2& uvMin, const Vec2& uvMax);
void AddVertsForOBB2D(std::vector<Vertex_PCU>& verts, const OBB2& obb2, const Rgba8& color);
void AddVertsForCapsule2D(std::vector<Vertex_PCU>& verts, const Capsule2& capsule, const Rgba8& color);
void AddVertsForTriangle2D(std::vector<Vertex_PCU>& verts, const Triangle2& triangle, const Rgba8& color);
void AddVertsForLineSegment2D(std::vector<Vertex_PCU>& verts, const LineSegment2& lineSegment, const Rgba8& color);
void AddVertsForArrow2D(std::vector<Vertex_PCU>& verts, Vec2 tailPos, Vec2 tipPos, float arrowSize, float lineThickness, const Rgba8& color);
void AddVertsForRoundedQuad3D(std::vector<Vertex_PCUTBN>& vertexes, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft, const Rgba8& color = Rgba8::WHITE,
                              const AABB2&                UVs = AABB2::ZERO_TO_ONE);
void AddVertsForQuad3D(std::vector<Vertex_PCU>& verts, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft, const Rgba8& color = Rgba8::WHITE,
                       const AABB2&             UVs                                                                                                                   = AABB2::ZERO_TO_ONE);
void AddVertsForQuad3D(std::vector<Vertex_PCUTBN>& verts, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft, const Rgba8& color = Rgba8::WHITE,
                       const AABB2&                UVs                                                                                                                   = AABB2::ZERO_TO_ONE);
void AddVertsForQuad3D(std::vector<Vertex_PCU>& outVerts, std::vector<unsigned int>& outIndices, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft,
                       const Rgba8&             color, const AABB2&                  uv);
void AddVertsForQuad3D(std::vector<Vertex_PCUTBN>& outVerts, std::vector<unsigned int>& outIndices, const Vec3& bottomLeft, const Vec3& bottomRight, const Vec3& topRight, const Vec3& topLeft,
                       const Rgba8&                color, const AABB2&                  uv);

void  TransformVertexArray3D(std::vector<Vertex_PCU>& verts, const Mat44& transform);
AABB2 GetVertexBounds2D(const std::vector<Vertex_PCU>& verts);
void  AddVertsForCylinder3D(std::vector<Vertex_PCU>& verts, const Vec3& start, const Vec3& end, float radius, const Rgba8& color = Rgba8::WHITE, const AABB2& UVs = AABB2::ZERO_TO_ONE,
                           int                       numSlices                                                                   = 32);
void AddVertsForCone3D(std::vector<Vertex_PCU>& verts, const Vec3& start, const Vec3& end, float radius, const Rgba8& color = Rgba8::WHITE, const AABB2& UVs = AABB2::ZERO_TO_ONE, int numSlices = 32);
void AddVertsForArrow3D(std::vector<Vertex_PCU>& verts, const Vec3& start, const Vec3& end, float radius, float arrowPercentage = 0.25, const Rgba8& color = Rgba8::WHITE, int numSlices = 32);
void AddVertsForArrow3DFixArrowSize(std::vector<Vertex_PCU>& verts, const Vec3& start, const Vec3& end, float radius, float arrowSize = 0.25f, const Rgba8& color = Rgba8::WHITE, int numSlices = 32);
void AddVertsForSphere3D(std::vector<Vertex_PCU>& verts, const Vec3& center, float radius, const Rgba8& color = Rgba8::WHITE, const AABB2& UVs = AABB2::ZERO_TO_ONE, int numSlices = 32,
                         int                      numStacks                                                   = 16);
void AddVertsForWireFrameSphere3D(std::vector<Vertex_PCU>& verts, const Vec3& center, float radius, const Rgba8& color = Rgba8::WHITE, const AABB2& UVs = AABB2::ZERO_TO_ONE, int numSlices = 16,
                                  int                      numStacks                                                   = 8);
void AddVertsForIndexedSphere3D(std::vector<Vertex_PCU>& verts, const Vec3& center, float radius, const Rgba8& color = Rgba8::WHITE, const AABB2& UVs = AABB2::ZERO_TO_ONE, int numSlices = 32,
                                int                      numStacks                                                   = 16);
void AddVertsForCube3D(std::vector<Vertex_PCU>& verts, const AABB3& box, const Rgba8& color, const AABB2& UVs = AABB2::ZERO_TO_ONE);
void AddVertsForCube3D(std::vector<Vertex_PCU>& verts, std::vector<unsigned int>& indexes, const AABB3& box, const Rgba8& color = Rgba8::WHITE, const AABB2& UVs = AABB2::ZERO_TO_ONE);
void AddVertsForCube3DWireFrame(std::vector<Vertex_PCU>& verts, const AABB3& box, const Rgba8& color, float thickness = 0.006f);
void AddVertsForCylinderZ3DWireFrame(std::vector<Vertex_PCU>& verts, const ZCylinder& cylinder, const Rgba8& color, int numSlices = 32);
void AddVertsForCylinderZ3D(std::vector<Vertex_PCU>& verts, ZCylinder cylinder, const Rgba8& color, const AABB2& UVs = AABB2::ZERO_TO_ONE, int numSlices = 32);

void AddVertsForOBB3D(std::vector<Vertex_PCU>& verts, const OBB3& obb3, const Rgba8& color = Rgba8::WHITE, const AABB2& UVs = AABB2::ZERO_TO_ONE);
void AddVertsForOBB3DWireFrame(std::vector<Vertex_PCU>& verts, const OBB3& obb3, const Rgba8& color = Rgba8::DEBUG_BLUE);
void AddVertsForPlane3D(std::vector<Vertex_PCU>& verts, const Plane3& plane3, const IntVec2& dimensions = IntVec2(50, 50), float thickness = 0.05f, const Rgba8& colorX = Rgba8::RED,
                        const Rgba8&             colorY                                                 = Rgba8::GREEN);

static Vec2 CalcRadialUVForCircle(const Vec3& pos, const Vec3& center, float radius, const Vec2& uvCenter, float uvRadius, float rotateDegrees = 0.f, bool flipAboutY = false);

[[maybe_unused]] void ConvertSingleVertex(const Vertex_PCU& src, Vertex_PCUTBN& dst);
[[maybe_unused]] void ConvertPCUArrayToPCUTBN(const Vertex_PCU* src, Vertex_PCUTBN* dst, size_t count);
