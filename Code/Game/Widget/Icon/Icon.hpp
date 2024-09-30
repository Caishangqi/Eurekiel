#pragma once
#include "Engine/Core/Vertex_PCU.hpp"

class BaseWidget;

struct Icon
{
public:

private:
    Vertex_PCU initialVertices[256] = {};
    Vertex_PCU vertices[256] = {};
    int numberOfVertices = 0;
    BaseWidget* owner = nullptr;
    float scale = 1;
    Vec2 position;
    Rgba8 color;
    float rotation;

public:
    Icon(Vertex_PCU* initialVertices, int numberOfVertices, BaseWidget* Owner);
    Vertex_PCU* GetInitialVertices();
    Vertex_PCU* GetVertices();
    int GetNumberOfVertices();

    void SetScale(float rate);
    void SetPosition(float x, float y);

    float GetScale();
    Vec2& GetPosition();

    void SetColor(Rgba8 color);
    Rgba8& GetColor();

    void SetRotation(float rotation);
    float GetRotation();
};
