#include "GameCommon.hpp"
#include "Game/Entity/Asteroid.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Renderer.hpp"

void DebugDrawRing(const Vec2& center, float radius, float thickness, const Rgba8& color)
{
    float           halfThickness = 0.5f * thickness;
    float           innerRadius   = radius - halfThickness;
    float           outerRadius   = radius + halfThickness;
    constexpr int   NUM_SIDES     = 32;
    constexpr int   NUM_TRIS      = 2 * NUM_SIDES;
    constexpr int   NUM_VERTS     = 3 * NUM_TRIS;
    Vertex_PCU      verts[NUM_VERTS];
    constexpr float DEGREES_PER_SIDE = 360.f / static_cast<float>(NUM_SIDES);
    for (int sideNum = 0; sideNum < NUM_SIDES; ++sideNum)
    {
        // Compute angle-related terms
        float startDegrees = DEGREES_PER_SIDE * static_cast<float>(sideNum);
        float endDegrees   = DEGREES_PER_SIDE * static_cast<float>(sideNum + 1);
        float cosStart     = CosDegrees(startDegrees);
        float sinStart     = SinDegrees(startDegrees);
        float cosEnd       = CosDegrees(endDegrees);
        float sinEnd       = SinDegrees(endDegrees);
        // Compute inner & outer positions
        Vec3 innerStartPos(center.x + innerRadius * cosStart, center.y + innerRadius * sinStart, 0.f);
        Vec3 outerStartPos(center.x + outerRadius * cosStart, center.y + outerRadius * sinStart, 0.f);
        Vec3 outerEndPos(center.x + outerRadius * cosEnd, center.y + outerRadius * sinEnd, 0.f);
        Vec3 innerEndPos(center.x + innerRadius * cosEnd, center.y + innerRadius * sinEnd, 0.f);
        // Trapezoid is made of two triangles; ABC and DEF
        // A is inner end; B is inner start; C is outer start
        // D is inner end; E is outer start; F is outer end
        int vertIndexA               = 6 * sideNum + 0;
        int vertIndexB               = 6 * sideNum + 1;
        int vertIndexC               = 6 * sideNum + 2;
        int vertIndexD               = 6 * sideNum + 3;
        int vertIndexE               = 6 * sideNum + 4;
        int vertIndexF               = 6 * sideNum + 5;
        verts[vertIndexA].m_position = innerEndPos;
        verts[vertIndexB].m_position = innerStartPos;
        verts[vertIndexC].m_position = outerStartPos;
        verts[vertIndexA].m_color    = color;
        verts[vertIndexB].m_color    = color;
        verts[vertIndexC].m_color    = color;
        verts[vertIndexD].m_position = innerEndPos;
        verts[vertIndexE].m_position = outerStartPos;
        verts[vertIndexF].m_position = outerEndPos;
        verts[vertIndexD].m_color    = color;
        verts[vertIndexE].m_color    = color;
        verts[vertIndexF].m_color    = color;
    }
    g_renderer->DrawVertexArray(NUM_VERTS, &verts[0]);
}

void DebugDrawLine(const Vec2& start, const Vec2& end, float thickness, const Rgba8& color)
{
    Vec2  dx            = end - start;
    float halfThickness = thickness * 0.5f;

    Vec2 stepForward = halfThickness * dx.GetNormalized();
    Vec2 stepLeft    = stepForward.GetRotated90Degrees();

    Vec2 EL = end + stepForward + stepLeft;
    Vec2 ER = end + stepForward - stepLeft;
    Vec2 SL = start - stepForward + stepLeft;
    Vec2 SR = start - stepForward - stepLeft;

    Vertex_PCU tempVerts[6] = {};
    tempVerts[0].m_color    = color;
    tempVerts[1].m_color    = color;
    tempVerts[2].m_color    = color;
    tempVerts[3].m_color    = color;
    tempVerts[4].m_color    = color;
    tempVerts[5].m_color    = color;

    tempVerts[0].m_position = Vec3(SR.x, SR.y, 0.f);
    tempVerts[1].m_position = Vec3(EL.x, EL.y, 0.f);
    tempVerts[2].m_position = Vec3(SL.x, SL.y, 0.f);

    tempVerts[3].m_position = Vec3(SR.x, SR.y, 0.f);
    tempVerts[4].m_position = Vec3(ER.x, ER.y, 0.f);
    tempVerts[5].m_position = Vec3(EL.x, EL.y, 0.f);

    g_renderer->DrawVertexArray(6, tempVerts);
}
