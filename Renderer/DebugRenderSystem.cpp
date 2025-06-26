#include "DebugRenderSystem.h"

#include "BitmapFont.hpp"
#include "Camera.hpp"
#include "Renderer.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/MathUtils.hpp"

///
struct DebugRenderPropsObject
{
    Rgba8                   startColor;
    Rgba8                   endColor;
    float                   m_liveSeconds       = 0.f;
    float                   m_duration          = 0.f;
    DebugRenderMode         m_mode              = DebugRenderMode::USE_DEPTH;
    bool                    m_isWired           = false;
    bool                    m_isWorldText       = false;
    bool                    m_isBillboard       = false;
    Vec3                    m_BillboardPosition = Vec3(0, 0, 0);
    std::vector<Vertex_PCU> vertices;

    Rgba8 xRayFirstPassColor;

    ~DebugRenderPropsObject()
    {
        vertices.clear();
    }
};

struct DebugRenderTextObject
{
    Rgba8                   startColor;
    Rgba8                   endColor;
    float                   m_liveSeconds = 0.f;
    float                   m_duration    = 0.f;
    std::vector<Vertex_PCU> vertices;
    std::string             m_text;
    bool                    m_isMessage = false;
};

///
STATIC DebugRenderConfig                    debugRenderConfig;
STATIC bool                                 isDebugModeVisible = true;
STATIC BitmapFont*                          debugBitmapFont    = nullptr;
STATIC std::vector<DebugRenderPropsObject*> debugRenderPropsList;
STATIC std::vector<DebugRenderTextObject*>  debugRenderTextList;
///
void DebugRenderSystemStartup(const DebugRenderConfig& config)
{
    debugRenderPropsList.reserve(100);
    debugRenderTextList.reserve(100);
    debugRenderConfig = config;
    debugBitmapFont   = config.m_renderer->CreateOrGetBitmapFont((debugRenderConfig.m_fontPath + debugRenderConfig.m_fontName).c_str());
    g_theEventSystem->SubscribeEventCallbackFunction("debugclear", Command_DebugRenderClear);
    g_theEventSystem->SubscribeEventCallbackFunction("debugtoggle", Command_DebugRenderToggle);
}

void DebugRenderSystemShutdown()
{
    for (DebugRenderPropsObject* debugRenderPropObject : debugRenderPropsList)
    {
        delete debugRenderPropObject;
        debugRenderPropObject = nullptr;
    }
}

void DebugRenderBeginFrame()
{
    float deltaSeconds = Clock::GetSystemClock().GetDeltaSeconds();

    // 使用 remove_if + erase 统一管理“该删除的对象”
    debugRenderPropsList.erase(
        std::remove_if(
            debugRenderPropsList.begin(),
            debugRenderPropsList.end(),
            [&](DebugRenderPropsObject* p)
            {
                // 1. 空指针直接移除（也可视需求返回 false，不做处理）
                if (!p)
                {
                    return true;
                }

                // 2. 若是无期限对象（m_duration == -1），跳过删除，也跳过下面的时间累加与插值
                if (p->m_duration == -1.f)
                {
                    return false; // 不删除，保留在容器
                }

                // 3. 累加存活时间
                p->m_liveSeconds += deltaSeconds;

                // 4. 检查是否超时，需要删除
                if (p->m_liveSeconds >= p->m_duration)
                {
                    delete p; // delete指针
                    return true; // true表示要从容器移除
                }

                // 5. 若还在存活且起止颜色不同，则更新插值
                if (p->startColor != p->endColor)
                {
                    float fraction = p->m_liveSeconds / p->m_duration;
                    for (Vertex_PCU& vertex : p->vertices)
                    {
                        vertex.m_color = Interpolate(p->startColor, p->endColor, fraction);
                    }
                }

                // false表示该元素继续保留在容器中
                return false;
            }
        ),
        debugRenderPropsList.end()
    );

    debugRenderTextList.erase(
        std::remove_if(
            debugRenderTextList.begin(),
            debugRenderTextList.end(),
            [&](DebugRenderTextObject* p)
            {
                if (!p)
                {
                    return true;
                }

                if (p->m_duration == -1.f)
                {
                    return false; // 不删除，保留在容器
                }

                p->m_liveSeconds += deltaSeconds;

                if (p->m_liveSeconds >= p->m_duration)
                {
                    delete p; // delete指针
                    return true; // true表示要从容器移除
                }

                if (p->startColor != p->endColor)
                {
                    float fraction = p->m_liveSeconds / p->m_duration;
                    for (Vertex_PCU& vertex : p->vertices)
                    {
                        vertex.m_color = Interpolate(p->startColor, p->endColor, fraction);
                    }
                }

                return false;
            }
        ),
        debugRenderTextList.end()
    );
}

DebugRenderPropsObject* CreateDebugRenderPropsObject(float duration, bool isWired, const Rgba8& startColor, const Rgba8& endColor, DebugRenderMode mode)
{
    auto debugRenderPropObject        = new DebugRenderPropsObject();
    debugRenderPropObject->m_duration = duration;
    debugRenderPropObject->m_mode     = mode;
    debugRenderPropObject->m_isWired  = isWired;
    debugRenderPropObject->startColor = startColor;
    debugRenderPropObject->endColor   = endColor;
    return debugRenderPropObject;
}

DebugRenderTextObject* CreateDebugRenderTextObject(float duration, const Rgba8& startColor, const Rgba8& endColor)
{
    auto debugRenderTextObject        = new DebugRenderTextObject();
    debugRenderTextObject->m_duration = duration;
    debugRenderTextObject->startColor = startColor;
    debugRenderTextObject->endColor   = endColor;
    return debugRenderTextObject;
}

void DebugRenderWorld(const Camera& camera)
{
    if (isDebugModeVisible == false)
        return;
    if (!debugRenderConfig.m_renderer)
        ERROR_AND_DIE("DebugRenderWorld: renderer is null")
    debugRenderConfig.m_renderer->BeginCamera(camera);
    for (DebugRenderPropsObject* debugRenderPropObject : debugRenderPropsList)
    {
        if (debugRenderPropObject == nullptr)
        {
            continue;
        }

        switch (debugRenderPropObject->m_mode)
        {
        case DebugRenderMode::ALWAYS:
            break;
        case DebugRenderMode::X_RAY:
            {
                debugRenderConfig.m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
                debugRenderConfig.m_renderer->BindTexture(nullptr);
                debugRenderConfig.m_renderer->SetBlendMode(BlendMode::ALPHA);
                debugRenderConfig.m_renderer->SetDepthMode(DepthMode::READ_ONLY_ALWAYS);
                Rgba8         originColor    = debugRenderPropObject->vertices[0].m_color;
                unsigned char newR           = static_cast<unsigned char>(static_cast<float>(originColor.r) * 0.8f);
                unsigned char newG           = static_cast<unsigned char>(static_cast<float>(originColor.g) * 0.8f);
                unsigned char newB           = static_cast<unsigned char>(static_cast<float>(originColor.b) * 0.8f);
                unsigned char newA           = static_cast<unsigned char>(static_cast<float>(originColor.a) * 0.8f);
                auto          firstPassColor = Rgba8(newR, newG, newB, newA);
                for (Vertex_PCU& vertex : debugRenderPropObject->vertices)
                {
                    vertex.m_color = firstPassColor;
                }
                debugRenderConfig.m_renderer->DrawVertexArray(debugRenderPropObject->vertices);
                for (Vertex_PCU& vertex : debugRenderPropObject->vertices)
                {
                    vertex.m_color = originColor;
                }
                debugRenderConfig.m_renderer->SetBlendMode(BlendMode::OPAQUE);
                debugRenderConfig.m_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
                debugRenderConfig.m_renderer->DrawVertexArray(debugRenderPropObject->vertices);
                break;
            }
        case DebugRenderMode::USE_DEPTH:
            debugRenderConfig.m_renderer->BindTexture(nullptr);
            debugRenderConfig.m_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);
            if (debugRenderPropObject->m_isWired)
            {
                debugRenderConfig.m_renderer->SetRasterizerMode(RasterizerMode::WIREFRAME_CULL_BACK);
            }
            else
            {
                debugRenderConfig.m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
            }
            if (debugRenderPropObject->m_isWorldText)
            {
                debugRenderConfig.m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
                debugRenderConfig.m_renderer->BindTexture(&debugBitmapFont->GetTexture());
            }
            if (debugRenderPropObject->m_isBillboard)
            {
                Mat44 cameraTransform = Mat44::MakeTranslation3D(camera.m_position);
                cameraTransform.Append(camera.m_orientation.GetAsMatrix_IFwd_JLeft_KUp());
                Mat44 billboardTransform;
                billboardTransform.Append(GetBillboardTransform(BillboardType::FULL_OPPOSING, cameraTransform, debugRenderPropObject->m_BillboardPosition));
                std::vector<Vertex_PCU> verts = debugRenderPropObject->vertices;
                TransformVertexArray3D(verts, billboardTransform);
                debugRenderConfig.m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_NONE);
                debugRenderConfig.m_renderer->BindTexture(&debugBitmapFont->GetTexture());
                debugRenderConfig.m_renderer->DrawVertexArray(verts);
                break;
            }

            debugRenderConfig.m_renderer->DrawVertexArray(debugRenderPropObject->vertices);
            break;
        }
    }
    debugRenderConfig.m_renderer->EndCamera(camera);
}

void DebugRenderScreen(const Camera& camera)
{
    if (!debugRenderConfig.m_renderer)
        ERROR_AND_DIE("DebugRenderScreen: renderer is null")
    debugRenderConfig.m_renderer->BeginCamera(camera);
    debugRenderConfig.m_renderer->SetRasterizerMode(RasterizerMode::SOLID_CULL_BACK);
    debugRenderConfig.m_renderer->SetBlendMode(BlendMode::OPAQUE);
    debugRenderConfig.m_renderer->SetDepthMode(DepthMode::READ_WRITE_LESS_EQUAL);

    debugRenderConfig.m_renderer->BindTexture(&debugBitmapFont->GetTexture());
    float                   currentHeight = camera.m_orthographicTopRight.y;
    std::vector<Vertex_PCU> debugMessageVerts;
    for (DebugRenderTextObject* debugRenderTextObject : debugRenderTextList)
    {
        if (debugRenderTextObject != nullptr)
        {
            if (!debugRenderTextObject->m_isMessage)
                debugRenderConfig.m_renderer->DrawVertexArray(debugRenderTextObject->vertices);
            if (debugRenderTextObject->m_isMessage)
            {
                debugBitmapFont->AddVertsForTextInBox2D(debugMessageVerts,
                                                        debugRenderTextObject->m_text,
                                                        AABB2(Vec2(0, currentHeight - 20.f),
                                                              Vec2(camera.m_orthographicTopRight.x, currentHeight)),
                                                        10,
                                                        debugRenderTextObject->startColor,
                                                        1,
                                                        Vec2(0.f, 0.5f));
                currentHeight -= 10.f;
            }
        }
    }
    debugRenderConfig.m_renderer->DrawVertexArray(debugMessageVerts);
    debugRenderConfig.m_renderer->BindTexture(nullptr);
    debugRenderConfig.m_renderer->EndCamera(camera);
}

void DebugRenderEndFrame()
{
}

void DebugAddWorldSphere(const Vec3& center, float radius, float duration, const Rgba8& startColor, const Rgba8& endColor, DebugRenderMode mode)
{
    DebugRenderPropsObject* debugRenderPropObject = CreateDebugRenderPropsObject(duration, false, startColor, endColor, mode);
    AddVertsForSphere3D(debugRenderPropObject->vertices, center, radius, startColor, AABB2::ZERO_TO_ONE, 16, 8);
    debugRenderPropsList.push_back(debugRenderPropObject);
}

void DebugAddWorldWireSphere(const Vec3& center, float radius, float duration, const Rgba8& startColor, const Rgba8& endColor, DebugRenderMode mode)
{
    DebugRenderPropsObject* debugRenderPropObject = CreateDebugRenderPropsObject(duration, true, startColor, endColor, mode);
    AddVertsForSphere3D(debugRenderPropObject->vertices, center, radius, startColor);
    debugRenderPropsList.push_back(debugRenderPropObject);
}

void DebugAddWorldCylinder(const Vec3& start, const Vec3& end, float radius, float duration, const Rgba8& startColor, const Rgba8& endColor, DebugRenderMode mode)
{
    auto debugRenderPropObject        = new DebugRenderPropsObject();
    debugRenderPropObject->m_duration = duration;
    debugRenderPropObject->m_mode     = mode;
    debugRenderPropObject->m_isWired  = true;
    debugRenderPropObject->startColor = startColor;
    debugRenderPropObject->endColor   = endColor;
    debugRenderPropsList.push_back(debugRenderPropObject);
    AddVertsForCylinder3D(debugRenderPropObject->vertices, start, end, radius, startColor);
}

void DebugAddWorldArrow(const Vec3& start, const Vec3& end, float radius, float duration, const Rgba8& startColor, const Rgba8& endColor, DebugRenderMode mode)
{
    DebugRenderPropsObject* debugRenderPropObject = CreateDebugRenderPropsObject(duration, false, startColor, endColor, mode);
    debugRenderPropsList.push_back(debugRenderPropObject);
    AddVertsForArrow3D(debugRenderPropObject->vertices, start, end, radius, 0.4f, startColor);
}

void DebugAddWorldArrowFixArrowSize(const Vec3&     start, const Vec3& end, float radius, float duration, float arrowSize, const Rgba8& startColor, const Rgba8& endColor, int numSlices,
                                    DebugRenderMode mode)
{
    DebugRenderPropsObject* debugRenderPropObject = CreateDebugRenderPropsObject(duration, false, startColor, endColor, mode);
    debugRenderPropsList.push_back(debugRenderPropObject);
    AddVertsForArrow3DFixArrowSize(debugRenderPropObject->vertices, start, end, radius, arrowSize, startColor, numSlices);
}

void DebugAddWorldArrow(const Vec3& start, const Vec3& end, float radius, float duration, float arrowPercent, const Rgba8& startColor, const Rgba8& endColor, DebugRenderMode mode)
{
    DebugRenderPropsObject* debugRenderPropObject = CreateDebugRenderPropsObject(duration, false, startColor, endColor, mode);
    debugRenderPropsList.push_back(debugRenderPropObject);
    AddVertsForArrow3D(debugRenderPropObject->vertices, start, end, radius, arrowPercent, startColor);
}

void DebugAddBasis(const Mat44& transform, float duration, float length, float radius, float colorScale, float alphaScale, DebugRenderMode mode)
{
    UNUSED(colorScale)
    UNUSED(alphaScale)
    Vec3 iBasis   = transform.GetIBasis3D();
    Vec3 jBasis   = transform.GetJBasis3D();
    Vec3 kBasis   = transform.GetKBasis3D();
    Vec3 location = transform.GetTranslation3D();
    DebugAddWorldArrow(Vec3(location + iBasis * length), Vec3(location), radius, duration, Rgba8::RED, Rgba8::RED, mode);
    DebugAddWorldArrow(Vec3(location + jBasis * length), Vec3(location), radius, duration, Rgba8::GREEN, Rgba8::GREEN, mode);
    DebugAddWorldArrow(Vec3(location + kBasis * length), Vec3(location), radius, duration, Rgba8::BLUE, Rgba8::BLUE, mode);
}

void DebugAddWorldBasis(const Mat44& transform, float duration, DebugRenderMode mode)
{
    Vec3 iBasis   = transform.GetIBasis3D();
    Vec3 jBasis   = transform.GetJBasis3D();
    Vec3 kBasis   = transform.GetKBasis3D();
    Vec3 location = transform.GetTranslation3D();
    DebugAddWorldArrow(Vec3(location + iBasis * 1), Vec3(location), 0.12f, duration, Rgba8::RED, Rgba8::RED, mode);
    DebugAddWorldArrow(Vec3(location + jBasis * 1), Vec3(location), 0.12f, duration, Rgba8::GREEN, Rgba8::GREEN, mode);
    DebugAddWorldArrow(Vec3(location + kBasis * 1), Vec3(location), 0.12f, duration, Rgba8::BLUE, Rgba8::BLUE, mode);
}

void DebugAddWorldText(const std::string& text, const Mat44& transform, float textHeight, const Rgba8& startColor, const Rgba8& endColor, DebugRenderMode mode, const Vec2& alignment, float duration)
{
    DebugRenderPropsObject* debugRenderPropObject = CreateDebugRenderPropsObject(duration, false, startColor, endColor, mode);
    debugRenderPropObject->m_isWorldText          = true;
    debugBitmapFont->AddVertsForText3DAtOriginXForward(debugRenderPropObject->vertices, textHeight / 4.0f, text, startColor, 1, alignment);
    TransformVertexArray3D(debugRenderPropObject->vertices, transform);
    debugRenderPropsList.push_back(debugRenderPropObject);
}

void DebugAddWorldBillboardText(const std::string& text, const Vec3& origin, float textHeight, const Rgba8& startColor, const Rgba8& endColor, DebugRenderMode mode, const Vec2& alignment,
                                float              duration)
{
    DebugRenderPropsObject* debugRenderPropObject = CreateDebugRenderPropsObject(duration, false, startColor, endColor, mode);
    debugRenderPropObject->m_isBillboard          = true;
    debugRenderPropObject->m_BillboardPosition    = origin;
    debugBitmapFont->AddVertsForText3DAtOriginXForward(debugRenderPropObject->vertices, textHeight, text, startColor, 1, alignment);
    debugRenderPropsList.push_back(debugRenderPropObject);
}

void DebugAddScreenText(const std::string& text, const AABB2& box, float cellHeight, float duration, const Rgba8& startColor, const Rgba8& endColor, const Vec2& alignment)
{
    std::vector<Vertex_PCU> vertices;
    DebugRenderTextObject*  debugRenderTextObject = CreateDebugRenderTextObject(duration, startColor, endColor);
    debugBitmapFont->AddVertsForTextInBox2D(debugRenderTextObject->vertices, text, box, cellHeight, startColor, 1, alignment);
    debugRenderTextList.push_back(debugRenderTextObject);
}

void DebugAddMessage(const std::string& text, float duration, const Rgba8& startColor, const Rgba8& endColor)
{
    DebugRenderTextObject* debugRenderTextObject = CreateDebugRenderTextObject(duration, startColor, endColor);
    debugRenderTextObject->m_text                = text;
    debugRenderTextObject->m_isMessage           = true;

    if (duration == 0.f)
    {
        /// Improve: perhaps need another way to improve the efficiency
        debugRenderTextList.insert(debugRenderTextList.begin(), debugRenderTextObject);
        return;
    }

    debugRenderTextList.push_back(debugRenderTextObject);
}

bool Command_DebugRenderClear(EventArgs& args)
{
    UNUSED(args)
    debugRenderPropsList.erase(debugRenderPropsList.begin(), debugRenderPropsList.end());
    return true;
}

bool Command_DebugRenderToggle(EventArgs& args)
{
    UNUSED(args)
    isDebugModeVisible = !isDebugModeVisible;
    return true;
}
