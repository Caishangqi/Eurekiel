#include "WidgetMainMenu.hpp"
#include <cmath>
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/SimpleTriangleFont.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Widget/Data/IconRes.hpp"
#include "Game/Widget/Icon/Icon.hpp"

WidgetMainMenu::WidgetMainMenu(WidgetHandler* handler): BaseWidget(handler)
{
    printf("[widget]        > WidgetMainMenu Registered!\n");
    icons[0] = new Icon(ICON::SPACESHIP, 36, this);
    icons[0]->SetScale(60);
    icons[0]->SetRotation(90);
    icons[0]->SetPosition(600, 500);

    icons[1] = new Icon(ICON::SPACESHIP, 36, this);
    icons[1]->SetScale(60);
    icons[1]->SetRotation(60);
    icons[1]->SetPosition(900, 500);
    active  = true;
    visible = true;

    name = "MainMenu";
}

WidgetMainMenu::~WidgetMainMenu()
{
}

void WidgetMainMenu::Update(float deltaSeconds)
{
    BaseWidget::Update(deltaSeconds);
    if (active)
    {
        internalCounter += deltaSeconds;
        // 基于 sin 函数的时间变化，来生成 0 到 1 的插值因子
        float time     = internalCounter; // 获取当前时间，或使用 deltaSeconds 进行累加
        float sinValue = (sin(time) + 1.f) * 0.5f; // 将 sin 的输出范围从 [-1, 1] 调整为 [0, 1]

        // 定义你想要的两个颜色
        auto color1 = Rgba8(153, 229, 80, 255); // 黑色
        auto color2 = Rgba8(91, 110, 225, 255); // 白色

        // 根据 sinValue 插值两个颜色之间的过渡
        interpolatedColor = Rgba8((color1.r + sinValue * (color2.r - color1.r)),
                                  (color1.g + sinValue * (color2.g - color1.g)),
                                  (color1.b + sinValue * (color2.b - color1.b)),
                                  255 // 透明度保持不变
        );

        // 设置 icon 的颜色
        Rgba8 icon0Color = interpolatedColor;
        icon0Color.r     = static_cast<char>(g_rng->RollRandomFloatZeroToOne());
        icons[0]->SetColor(icon0Color);
        icons[1]->SetColor(interpolatedColor);
        float randomFloat1 = g_rng->RollRandomFloatInRange(-100, 100);

        icons[0]->SetRotation(icons[0]->GetRotation() + deltaSeconds * randomFloat1);
        float randomFloat2 = g_rng->RollRandomFloatInRange(-100, 100);
        icons[1]->SetRotation(icons[1]->GetRotation() + deltaSeconds * randomFloat2);
    }
}

void WidgetMainMenu::Render()
{
    BaseWidget::Render();
    if (visible)
    {
        for (Icon*& icon : icons)
        {
            if (icon != nullptr)
                g_renderer->DrawVertexArray(icon->GetNumberOfVertices(), icon->GetVertices());
        }

        std::vector<Vertex_PCU> textVerts;
        AddVertsForTextTriangles2D(textVerts, "Press Any Button To Start!", Vec2(420.f, 60.f), 40.f,
                                   Rgba8(255, 150, 50));
        for (Vertex_PCU& text_vert : textVerts)
        {
            text_vert.m_color = interpolatedColor;
        }
        g_renderer->DrawVertexArray(textVerts.size(), textVerts.data());

        std::vector<Vertex_PCU> textVertsVersion;
        AddVertsForTextTriangles2D(textVertsVersion, "Beta 0.1.2 (Gold)", Vec2(10.f, 10.f), 20.f,
                                   Rgba8(255, 255, 255));
        g_renderer->DrawVertexArray(textVertsVersion.size(), textVertsVersion.data());
    }
}
