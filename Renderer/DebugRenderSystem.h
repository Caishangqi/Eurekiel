#pragma once
#include <string>

#include "BitmapFont.hpp"
#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"


class Camera;
class Renderer;
class AABB2;
struct Mat44;
struct Vec3;

enum class DebugRenderMode
{
    ALWAYS,
    USE_DEPTH,
    X_RAY,
};

struct DebugRenderConfig
{
    Renderer*   m_renderer = nullptr;
    std::string m_fontPath = ".enigma/data/Fonts/";
    std::string m_fontName = "SquirrelFixedFont";
};

// Setup
void DebugRenderSystemStartup(const DebugRenderConfig& config);
void DebugRenderSystemShutdown();

// Control
void DebugRenderSetVisible();
void DebugRenderSetHidden();
void DebugRenderClear();

// Output
void DebugRenderBeginFrame();
void DebugRenderWorld(const Camera& camera);
void DebugRenderScreen(const Camera& camera);
void DebugRenderEndFrame();

// Geometry
void DebugAddWorldSphere(
    const Vec3&     center,
    float           radius,
    float           duration,
    const Rgba8&    startColor = Rgba8::WHITE,
    const Rgba8&    endColor   = Rgba8::WHITE,
    DebugRenderMode mode       = DebugRenderMode::USE_DEPTH
);

void DebugAddWorldWireSphere(
    const Vec3&     center,
    float           radius,
    float           duration,
    const Rgba8&    startColor = Rgba8::WHITE,
    const Rgba8&    endColor   = Rgba8::WHITE,
    DebugRenderMode mode       = DebugRenderMode::USE_DEPTH
);

void DebugAddWorldCylinder(
    const Vec3&     start,
    const Vec3&     end,
    float           radius,
    float           duration,
    const Rgba8&    startColor = Rgba8::WHITE,
    const Rgba8&    endColor   = Rgba8::WHITE,
    DebugRenderMode mode       = DebugRenderMode::USE_DEPTH
);

void DebugAddWorldWireCylinder(
    const Vec3&     start,
    const Vec3&     end,
    float           radius,
    float           duration,
    const Rgba8&    startColor = Rgba8::WHITE,
    const Rgba8&    endColor   = Rgba8::WHITE,
    DebugRenderMode mode       = DebugRenderMode::USE_DEPTH
);

void DebugAddWorldArrow(
    const Vec3&     start,
    const Vec3&     end,
    float           radius,
    float           duration,
    const Rgba8&    startColor = Rgba8::WHITE,
    const Rgba8&    endColor   = Rgba8::WHITE,
    DebugRenderMode mode       = DebugRenderMode::USE_DEPTH
);

void DebugAddWorldArrowFixArrowSize(
    const Vec3&     start,
    const Vec3&     end,
    float           radius,
    float           duration,
    float           arrowSize  = 0.25f,
    const Rgba8&    startColor = Rgba8::WHITE,
    const Rgba8&    endColor   = Rgba8::WHITE,
    int             numSlices  = 32,
    DebugRenderMode mode       = DebugRenderMode::USE_DEPTH
);

void DebugAddWorldWireArrow(
    const Vec3&     start,
    const Vec3&     end,
    float           radius,
    float           duration,
    const Rgba8&    startColor = Rgba8::WHITE,
    const Rgba8&    endColor   = Rgba8::WHITE,
    DebugRenderMode mode       = DebugRenderMode::USE_DEPTH
);

void DebugAddBasis(
    const Mat44&    transform,
    float           duration,
    float           length,
    float           radius,
    float           colorScale = 1.0f,
    float           alphaScale = 1.0f,
    DebugRenderMode mode       = DebugRenderMode::USE_DEPTH
);

void DebugAddWorldBasis(
    const Mat44&    transform,
    float           duration,
    DebugRenderMode mode = DebugRenderMode::USE_DEPTH
);

void DebugAddWorldText(
    const std::string& text,
    const Mat44&       transform,
    float              textHeight,
    const Rgba8&       startColor = Rgba8::WHITE,
    const Rgba8&       endColor   = Rgba8::WHITE,
    DebugRenderMode    mode       = DebugRenderMode::USE_DEPTH,
    const Vec2&        alignment  = Vec2(0.5f, 0.5f),
    float              duration   = 0.0f
);

void DebugAddWorldBillboardText(
    const std::string& text,
    const Vec3&        origin,
    float              textHeight,
    const Rgba8&       startColor = Rgba8::WHITE,
    const Rgba8&       endColor   = Rgba8::WHITE,
    DebugRenderMode    mode       = DebugRenderMode::USE_DEPTH,
    const Vec2&        alignment  = Vec2(0.0f, 0.0f),
    float              duration   = 0.0f
);

void DebugAddScreenText(
    const std::string& text,
    const AABB2&       box,
    float              cellHeight,
    float              duration,
    const Rgba8&       startColor = Rgba8::WHITE,
    const Rgba8&       endColor   = Rgba8::WHITE,
    const Vec2&        alignment  = Vec2(1.0f, 1.0f)
);


void DebugAddMessage(
    const std::string& text,
    float              duration,
    const Rgba8&       startColor = Rgba8::WHITE,
    const Rgba8&       endColor   = Rgba8::WHITE
);

// Console commands
bool Command_DebugRenderClear(EventArgs& args);
bool Command_DebugRenderToggle(EventArgs& args);
