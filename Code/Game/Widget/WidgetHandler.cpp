#include "WidgetHandler.hpp"
#include "stdio.h"
#include "Widgets/WidgetInGameScoreBoard.hpp"
#include "Widgets/WidgetMainMenu.hpp"
#include "Widgets/WidgetPlayerHealth.hpp"


WidgetHandler::~WidgetHandler()
{
}

WidgetHandler::WidgetHandler(Game* owner)
{
    this->owner = owner;
    printf("[widget]    Widget Handler Init prepare registry widgets...\n");
    m_registerWidget[0] = new WidgetPlayerHealth(this);
    printf("[widget]    Widget Handler Init prepare registry widgets...\n");
    m_registerWidget[1] = new WidgetMainMenu(this);
    printf("[widget]    Widget Handler Init prepare registry widgets...\n");
    m_registerWidget[2] = new WidgetInGameScoreBoard(this);
}

void WidgetHandler::Update(float deltaSeconds)
{
    for (BaseWidget* m_register_widget : m_registerWidget)
    {
        if (m_register_widget)
            m_register_widget->Update(deltaSeconds);
    }
}

void WidgetHandler::Draw()
{
}

void WidgetHandler::Render()
{
    for (BaseWidget* m_register_widget : m_registerWidget)
    {
        if (m_register_widget)
            m_register_widget->Render();
    }
}

BaseWidget* WidgetHandler::GetWidgetByName(std::string widgetName)
{
    for (BaseWidget* m_register_widget : m_registerWidget)
    {
        if (m_register_widget != nullptr && m_register_widget->getName() == widgetName)
        {
            return m_register_widget;
        }
    }
    return nullptr;
}
