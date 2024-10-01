#pragma once

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

public:
    WidgetPlayerHealth* playerHealthWidget;
    WidgetMainMenu* mainMenuWidget;
    Game* owner;
};
