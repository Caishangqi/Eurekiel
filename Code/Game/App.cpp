#include "Game/App.hpp"

Renderer* g_renderer;
App* g_theApp;

App::App()
{
    m_gameCamera = new Camera();
}

App::~App()
{
    // Delete and release the render instance
    delete g_renderer;
    g_renderer = nullptr;

    delete m_gameCamera;
    m_gameCamera = nullptr;
}

void App::Startup()
{
    g_renderer = new Renderer(); // Create render
    g_renderer->StartUp();

    m_shipPos = Vec2{0.0f, 50.0f};
}

void App::Shutdown()
{
    g_renderer->Shutdown();
}

void App::RunFrame()
{
    BeginFrame();
    Update(1.f / 60.f);
    Render();
    EndFrame();
}

bool App::IsQuitting() const
{
    return m_isQuitting;
}

bool App::HandleKeyPress(unsigned char keyCode)
{
    // #SD1ToDo: Tell the App (or InputSystem later) about this key-pressed event...
    if (keyCode == 'Q') // #SD1ToDo: move this "check for ESC pressed" code to App
    {
        m_isQuitting = true;
        return false; // "Consumes" this message (tells Windows "okay, we handled it")
    }

    if (keyCode == 'P')
    {
        m_isPaused = !m_isPaused; // Toggles between pause state
        return false;
    }

    if (keyCode == 'T')
    {
        m_isSlowMo = true;
    }

    if (keyCode == 'O')
    {
        m_isPausedAfterFrame = true;
    }
    return false;
}

bool App::HandleKeyRelease(unsigned char keyCode)
{
    if (keyCode == 'T')
    {
        m_isSlowMo = false;
    }
    return false;
}

bool App::HandleQuitRequested()
{
    m_isQuitting = true;
    return true;
}


void App::BeginFrame()
{
    g_renderer->BeingFrame();
}

void App::Update(float deltaSeconds)
{
    m_gameCamera->SetOrthoView(Vec2(0, 0), Vec2(200, 100));

    if (m_isSlowMo)
    {
        deltaSeconds *= 0.1f;
    }

    if (m_isPaused)
    {
        deltaSeconds = 0.0f;
    }

    UpdateShip(deltaSeconds);

    if (m_isPausedAfterFrame)
    {
        if (m_isPaused)
        {
            m_isPaused = false;
            m_isPausedAfterFrame = true;
        }
        else
        {
            m_isPaused = true;
            m_isPausedAfterFrame = false;
        }
    }
}

void App::Render() const
{
    g_renderer->ClearScreen(Rgba8(100.f, 50.f, 0.0f, 1.f));
    g_renderer->BeingCamera(*m_gameCamera);
    RenderShip();
    g_renderer->EndCamera(*m_gameCamera);
}

void App::EndFrame()
{
    g_renderer->EndFrame();
}

void App::UpdateShip(float deltaSeconds)
{
    float movePerSecond = 20.0f;
    float movePerFrame = movePerSecond * deltaSeconds;

    m_shipPos.x += movePerFrame;

    if (m_shipPos.x >= 200.f) // TODO: Math this a named constant
    {
        m_isQuitting = true;
    }
}

void App::RenderShip() const
{
    Vertex_PCU verts[3] = {
        Vertex_PCU(Vec3(m_shipPos.x + 4.f, m_shipPos.y, 0.0f), Rgba8(255, 255, 255, 255), Vec2(0.0f, 0.0f)),
        Vertex_PCU(Vec3(m_shipPos.x - 2.f, m_shipPos.y + 2.f, 0.0f), Rgba8(0, 127, 255, 255), Vec2(0.0f, 0.0f)),
        Vertex_PCU(Vec3(m_shipPos.x - 2.f, m_shipPos.y - 2.f, 0.0f), Rgba8(0, 0, 0, 255), Vec2(0.f, 0.f))
    };
    g_renderer->DrawVertexArray(3, verts);
}
