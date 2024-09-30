#include "WidgetMainMenu.hpp"

#include <cmath>

#include "stdio.h"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Widget/Data/IconRes.hpp"
#include "Game/Widget/Icon/Icon.hpp"

WidgetMainMenu::WidgetMainMenu(WidgetHandler* handler): BaseWidget(handler)
{
    printf("[widget]        > WidgetMainMenu Registered!\n");
    icons[0] = new Icon(ICON::SPACESHIP, 36, this);
    icons[0]->SetRotation(90);
    icons[0]->SetScale(5);
    icons[0]->SetPosition(50, 50);
    active = true;
    visible = true;
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
        float time = static_cast<float>(internalCounter); // 获取当前时间，或使用 deltaSeconds 进行累加
        float sinValue = (sin(time) + 1.f) * 0.5f; // 将 sin 的输出范围从 [-1, 1] 调整为 [0, 1]

        // 定义你想要的两个颜色
        Rgba8 color1 = Rgba8(153, 229, 80, 255); // 黑色
        Rgba8 color2 = Rgba8(91, 110, 225, 255); // 白色

        // 根据 sinValue 插值两个颜色之间的过渡
        Rgba8 interpolatedColor = Rgba8((color1.r + sinValue * (color2.r - color1.r)),
                                        (color1.g + sinValue * (color2.g - color1.g)),
                                        (color1.b + sinValue * (color2.b - color1.b)),
                                        255 // 透明度保持不变
        );

        // 设置 icon 的颜色
        icons[0]->SetColor(interpolatedColor);
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
    }
}
