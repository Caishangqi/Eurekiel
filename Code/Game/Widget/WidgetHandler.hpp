#pragma once
#include <string>

class BaseWidget;
class WidgetMainMenu;
class WidgetPlayerHealth;
class Game;

class WidgetHandler
{
public:
    ~WidgetHandler();
    WidgetHandler(Game* owner);
    void Update(float deltaSeconds);
    void Draw();
    void Render();

    BaseWidget * GetWidgetByName(std::string widgetName);
    
    BaseWidget* m_registerWidget[128] = {};

    Game*               owner;
};
