#pragma once
#include "BaseWidget.hpp"
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
};
