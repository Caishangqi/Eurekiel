#include "WidgetHandler.hpp"
#include "stdio.h"
#include "Widgets/WidgetMainMenu.hpp"
#include "Widgets/WidgetPlayerHealth.hpp"

WidgetHandler::WidgetHandler(Game* owner)
{
    this->owner = owner;
    printf("[widget]    Widget Handler Init prepare registry widgets...\n");
    playerHealthWidget = new WidgetPlayerHealth(this);
    printf("[widget]    Widget Handler Init prepare registry widgets...\n");
    mainMenuWidget = new WidgetMainMenu(this);
}

void WidgetHandler::Update(float deltaSeconds)
{
    playerHealthWidget->Update(deltaSeconds);
    mainMenuWidget->Update(deltaSeconds);
}

void WidgetHandler::Draw()
{
}

void WidgetHandler::Render()
{
    playerHealthWidget->Render();
    mainMenuWidget->Render();
}
