#include "Game/App.hpp"

#include <cstdio>

#include "Engine/Math/RandomNumberGenerator.hpp"

Renderer* g_renderer = nullptr;
App* g_theApp = nullptr;
RandomNumberGenerator* g_rng = nullptr;

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

    /*  h. App::Startup() should do the following, in this order:
        i. Create a new instance of Renderer (and assign its memory address to g_theRenderer)
        ii. Call g_theRenderer->Startup()
        iii. Create a new instance of Game (and assign its memory address to the App’s m_game)
    */
    m_theGame = new Game();
    g_rng = new RandomNumberGenerator();

    m_shipPos = Vec2{0.0f, 50.0f};
}

void App::Shutdown()
{
    /*
    *   i. App::Shutdown() should do the opposite of each Startup step, in the reverse order:
        i. Delete (and null) m_theGame
        ii. Call g_theRenderer->Shutdown()
        iii. Delete (and null) g_theRenderer
     */
    delete m_theGame;
    m_theGame = nullptr;
    g_renderer->Shutdown();
    delete g_renderer;
    g_renderer = nullptr;
}

void App::RunFrame()
{
    BeginFrame(); //Engine pre-frame stuff
    Update(1.f / 60.f); // Game updates / moves / spawns / hurts
    Render(); // Game draws current state of things
    EndFrame(); // Engine post-frame
}

bool App::IsQuitting() const
{
    return m_isQuitting;
}

void App::HandleKeyPress(unsigned char keyCode)
{
    m_areKeysDown[keyCode] = true;

    /*// #SD1ToDo: Tell the App (or InputSystem later) about this key-pressed event...
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
    return false;*/
}

void App::HandleKeyRelease(unsigned char keyCode)
{
    /*if (keyCode == 'T')
    {
        m_isSlowMo = false;
    }
    return false;*/
    m_areKeysDown[keyCode] = false;
}

void App::HandleQuitRequested()
{
    m_isQuitting = true;
}

bool App::IsKeyDown(unsigned char keyCode) const
{
    return m_areKeysDown[keyCode];
}

bool App::WasKeyJustPressed(unsigned char keyCode) const
{
    bool isKeyDown = m_areKeysDown[keyCode];
    bool wasKeyJustPressed = m_areKeysDownLastFrame[keyCode];
    return isKeyDown && !wasKeyJustPressed;
}

void App::HandleKeyMapping()
{
    m_isQuitting = IsKeyDown('Q');
    if (WasKeyJustPressed('I'))
        m_theGame->SpawnNewAsteroids();
}

void App::AdjustForPauseAndTimeDistortion(float& deltaSeconds)
{
    m_isSlowMo = IsKeyDown('T');
    if (m_isSlowMo)
    {
        deltaSeconds *= 0.1f;
    }


    if (WasKeyJustPressed('P'))
    {
        m_isPaused = !m_isPaused;
    }

    if (m_isPaused)
    {
        deltaSeconds = 0.f;
    }
}


void App::BeginFrame()
{
    g_renderer->BeingFrame();
    // g_theInput->BeingFrame();
}

void App::UpdateCameras()
{
    m_gameCamera->SetOrthoView(Vec2(0, 0), Vec2(200, 100));
}

void App::Update(float deltaSeconds)
{
    HandleKeyMapping();
    AdjustForPauseAndTimeDistortion(deltaSeconds);

    m_theGame->Update(deltaSeconds);
    //  UpdateShip(deltaSeconds);
    UpdateCameras();
}

// If this methods is const, the methods inn the method should also promise
// const
void App::Render() const
{
    g_renderer->ClearScreen(Rgba8(0.f, 0.f, 0.0f, 1.f));
    g_renderer->BeingCamera(*m_gameCamera);

    m_theGame->Render();
    //RenderShip();
    g_renderer->EndCamera(*m_gameCamera);
}

void App::EndFrame()
{
    g_renderer->EndFrame();
    // g_theInput->EndFrame();

    // Dump() 的时候
    // Copy m_areKeysDown to m_wereKeyDownLastFrame
    for (int keyIndex = 0; keyIndex < 256; ++keyIndex)
    {
        m_areKeysDownLastFrame[keyIndex] = m_areKeysDown[keyIndex];
    }
    //m_areKeysDownLastFrame = m_areKeysDown;
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
