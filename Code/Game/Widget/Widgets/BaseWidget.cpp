#include "BaseWidget.hpp"

BaseWidget::BaseWidget(WidgetHandler* handler)
{
    this->handler = handler;
}

BaseWidget::~BaseWidget()
{
}

std::string BaseWidget::getName()
{
    return name;
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
    this->active  = true;
    this->visible = true;
}

void BaseWidget::SetInActiveAndInVisible()
{
    this->active  = false;
    this->visible = false;
}
