#include "BaseWidget.hpp"

BaseWidget::BaseWidget(WidgetHandler* handler)
{
    this->handler = handler;
}

BaseWidget::~BaseWidget()
{
}

void BaseWidget::Draw()
{
}

void BaseWidget::Update(float deltaSeconds)
{
}

void BaseWidget::Render()
{
    if (visible == false)
    {
        return;
    }
}

void BaseWidget::SetVisibility(bool visible)
{
    this->visible = visible;
}

void BaseWidget::SetActive(bool active)
{
    this->active = active;
}

void BaseWidget::Reset()
{
}

void BaseWidget::SetActiveAndVisible()
{
    this->active = true;
    this->visible = true;
}
