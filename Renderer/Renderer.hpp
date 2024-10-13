#pragma once

struct Vertex_PCU;
class Camera;
struct Rgba8;

class Renderer
{
public:
    void Startup();
    void BeginFrame();
    void EndFrame();
    void Shutdown();

    void ClearScreen(const Rgba8& clearColor);
    void BeingCamera(const Camera& camera);
    void EndCamera(const Camera& camera);
    void DrawVertexArray(int numVertexes, const Vertex_PCU* vertexes);

private:
    // Private (internal) member function will go here
    void* m_apiRenderingContext = nullptr; // ...becomes void* Renderer::m_apiRenderingContext


    // Private (internal) data members will go here
    void CreateRenderingContext();
};
