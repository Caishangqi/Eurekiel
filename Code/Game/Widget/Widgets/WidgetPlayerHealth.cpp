#include "WidgetPlayerHealth.hpp"
#include "stdio.h"
#include "Engine/Renderer/Renderer.hpp"
#include "Game/GameCommon.hpp"
#include "Game/Widget/Data/IconRes.hpp"

WidgetPlayerHealth::WidgetPlayerHealth(WidgetHandler* handler)
    : BaseWidget(handler)
{
    printf("[widget]        > WidgetPlayerHealth Registered!\n");
    for (int i = 0; i < NUM_MAX_TRY - 1; i++)
    {
        icons[i] = new Icon(ICON::SPACESHIP, 15, this);
        icons[i]->SetRotation(90); // first rotation
        icons[i]->SetPosition(6 + static_cast<float>(6 * i), 96);
    }
    active  = false;
    visible = false;
}

WidgetPlayerHealth::~WidgetPlayerHealth()
{
    for (Icon* element : icons)
    {
        delete element;
        element = nullptr;
    }
}

void WidgetPlayerHealth::OnPlayerShipRespawn(PlayerShip* playerShip, int remainTry)
{
    for (Icon*& element : icons)
    {
        delete element;
        element = nullptr;
    }
    if (active && visible)
    {
        for (int i = 0; i < remainTry - 1; i++)
        {
            icons[i] = new Icon(ICON::SPACESHIP, 15, this);
            icons[i]->SetRotation(90); // first rotation
            icons[i]->SetPosition(6 + static_cast<float>(6 * i), 96);
        }
    }
}

void WidgetPlayerHealth::Update(float deltaSeconds)
{
    BaseWidget::Update(deltaSeconds);
    if (active)
    {
    }
}

void WidgetPlayerHealth::Render()
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

void WidgetPlayerHealth::Reset()
{
    BaseWidget::Reset();
    for (int i = 0; i < NUM_MAX_TRY - 1; i++)
    {
        icons[i] = new Icon(ICON::SPACESHIP, 15, this);
        icons[i]->SetRotation(90); // first rotation
        icons[i]->SetPosition(6 + static_cast<float>(6 * i), 96);
    }
}
