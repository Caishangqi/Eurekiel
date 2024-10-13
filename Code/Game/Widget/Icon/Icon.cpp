#include "Icon.hpp"
#include "Engine/Math/MathUtils.hpp"

Icon::Icon(Vertex_PCU* initialVertices, int numberOfVertices, BaseWidget* Owner)
{
    for (int i = 0; i < numberOfVertices; i++)
    {
        this->initialVertices[i] = initialVertices[i];
        this->vertices[i]        = initialVertices[i];
    }
    this->owner            = Owner;
    this->numberOfVertices = numberOfVertices;
    position               = Vec2(0.0f, 0.0f);
    scale                  = 1.0f;
}

Vertex_PCU* Icon::GetInitialVertices()
{
    return initialVertices;
}

Vertex_PCU* Icon::GetVertices()
{
    return vertices;
}

int Icon::GetNumberOfVertices()
{
    return numberOfVertices;
}

void Icon::SetScale(float rate)
{
    for (int i = 0; i < numberOfVertices; i++)
    {
        vertices[i].m_position.x = vertices[i].m_position.x * rate;
        vertices[i].m_position.y = vertices[i].m_position.y * rate;
    }
}

void Icon::SetPosition(float x, float y)
{
    position.x = x;
    position.y = y;
    for (int i = 0; i < numberOfVertices; i++)
    {
        vertices[i].m_position.x = vertices[i].m_position.x + x;
        vertices[i].m_position.y = vertices[i].m_position.y + y;
    }
}

float Icon::GetScale()
{
    return scale;
}

Vec2& Icon::GetPosition()
{
    return position;
}

void Icon::SetColor(Rgba8 color)
{
    for (int i = 0; i < numberOfVertices; i++)
    {
        vertices[i].m_color = color;
    }
}

Rgba8& Icon::GetColor()
{
    return color;
}

void Icon::SetRotation(float rotation)
{
    for (int i = 0; i < numberOfVertices; i++)
    {
        this->rotation = rotation;
        TransformPositionXY3D(vertices[i].m_position, 1.0f, rotation, Vec2(0, 0));
    }
}

float Icon::GetRotation()
{
    return rotation;
}
