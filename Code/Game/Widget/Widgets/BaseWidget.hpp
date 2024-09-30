#pragma once

class WidgetHandler;
struct Icon;

class BaseWidget
{
public:
    BaseWidget(WidgetHandler* handler);
    virtual ~BaseWidget();

protected:
    Icon* icons[16] = {};
    WidgetHandler* handler = nullptr;
    bool visible = false; // whether or not render by opengl
    bool active = false; // whether or not handle logic

public:
    virtual void Draw();
    virtual void Update(float deltaSeconds);
    virtual void Render();
    virtual void SetVisibility(bool visible);
    virtual void SetActive(bool active);

    // Reset the whole widget
    // TODO: should recreate the widget rather than reset
    virtual void Reset();

    virtual void SetActiveAndVisible();
};
