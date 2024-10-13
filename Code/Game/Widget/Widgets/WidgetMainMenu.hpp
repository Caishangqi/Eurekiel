#pragma once
#include "BaseWidget.hpp"
#include "Engine/Core/Rgba8.hpp"
struct Icon;
class WidgetMainMenu : public BaseWidget
{
public:
    WidgetMainMenu(WidgetHandler* handler);
    ~WidgetMainMenu() override;

    void Update(float deltaSeconds) override;
    void Render() override;

private:
    float internalCounter;
    Rgba8 interpolatedColor;
};
