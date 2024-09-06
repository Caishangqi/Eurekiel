#pragma once

#include "Engine/Renderer/Camera.hpp"
#include "Engine/Core/Vertex_PCU.hpp"

class Renderer
{
public:
    void StartUp();
    void BeingFrame();
    void EndFrame();
    void Shutdown();

    void ClearScreen(const Rgba8& clearColor);
    void BeingCamera(const Camera& camera);
    void EndCamera(const Camera& camera);
    void DrawVertexArray(int numVertexes, Vertex_PCU const* vertexes);
private:
    // Private (internal) member function will go here
    void * m_apiRenderingContext = nullptr; // ...becomes void* Renderer::m_apiRenderingContext


private:
    // Private (internal) data members will go here
    void CreateRenderingContext();

    
};
