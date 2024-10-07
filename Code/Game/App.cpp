#include "Game/App.hpp"

#include "Engine/Audio/AudioSystem.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"

Renderer*              g_renderer = nullptr;
App*                   g_theApp   = nullptr;
RandomNumberGenerator* g_rng      = nullptr;
InputSystem*           g_theInput = nullptr;
AudioSystem*           g_theAudio = nullptr;

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

    delete g_theInput;
    g_theInput = nullptr;

    delete m_resourceManager;
    m_resourceManager = nullptr;
}

void App::Startup()
{
    g_renderer = new Renderer(); // Create render
    g_renderer->Startup();

    // g_theInput Start Up
    g_theInput = new InputSystem();
    g_theInput->Startup();

    g_theAudio = new AudioSystem();
    g_theAudio->Startup();

    m_resourceManager = new ResourceManager();
    m_resourceManager->Startup();

    /*  h. App::Startup() should do the following, in this order:
        i. Create a new instance of Renderer (and assign its memory address to g_theRenderer)
        ii. Call g_theRenderer->Startup()
        iii. Create a new instance of Game (and assign its memory address to the App’s m_game)
    */
    m_theGame = new Game();
    g_rng     = new RandomNumberGenerator();
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
    float timeNow        = static_cast<float>(GetCurrentTimeSeconds());
    float deltaSeconds   = timeNow - m_LastFrameStartTime;
    m_LastFrameStartTime = timeNow;

    BeginFrame(); //Engine pre-frame stuff
    Update(deltaSeconds); // Game updates / moves / spawns / hurts
    Render(); // Game draws current state of things
    EndFrame(); // Engine post-frame
}

bool App::IsQuitting() const
{
    return m_isQuitting;
}

void App::HandleQuitRequested()
{
    m_isQuitting = true;
}

void App::HandleKeyBoardEvent()
{
    // ESC compatibility controller
    if (g_theInput->GetController(0).WasButtonJustPressed(XBOX_BUTTON_B))
    {
        if (m_theGame->IsInMainMenu)
        {
            m_isQuitting = true;
        }
    }

    // ESC
    if (g_theInput->WasKeyJustPressed(27))
    {
        if (m_theGame->IsInMainMenu)
        {
            m_isQuitting = true;
        }
    }

    if (g_theInput->WasKeyJustPressed('I'))
        m_theGame->SpawnNewAsteroids();
    if (g_theInput->WasKeyJustPressed(0x70))
    {
        m_isDebug = !m_isDebug;
    }
    if (g_theInput->WasKeyJustPressed(0x77))
    {
        m_isPendingRestart = true;
    }
    if (g_theInput->WasKeyJustPressed('O'))
    {
        m_theGame->Update(1 / 60.f);
        m_isPaused = true;
    }
}

void App::AdjustForPauseAndTimeDistortion(float& deltaSeconds)
{
    m_isSlowMo = g_theInput->IsKeyDown('T');
    if (m_isSlowMo)
    {
        deltaSeconds *= 0.1f;
    }


    if (g_theInput->WasKeyJustPressed('P'))
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
    g_renderer->BeginFrame();
    g_theInput->BeginFrame();
    // g_theAudio->BeginFrame();
    // g_theNetwork->BeginFrame();
    // g_theWindow->BeginFrame();
    // g_theDevConsole->BeginFrame();
    // g_theEventSystem->BeginFrame();
    // g_theNetwork->BeginFrame();
}

void App::UpdateCameras()
{
    m_gameCamera->SetOrthoView(Vec2(0, 0), Vec2(200, 100));
}

void App::Update(float deltaSeconds)
{
    HandleKeyBoardEvent();
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
    g_theInput->EndFrame();

    // Dump() 的时候
    // Copy m_areKeysDown to m_wereKeyDownLastFrame
    for (int keyIndex = 0; keyIndex < 256; ++keyIndex)
    {
        m_areKeysDownLastFrame[keyIndex] = m_areKeysDown[keyIndex];
    }

    if (m_isPendingRestart)
    {
        delete m_theGame;
        m_theGame          = nullptr;
        m_isPendingRestart = false;
        m_theGame          = new Game();
    }
}

void App::UpdateShip(float deltaSeconds)
{
    float movePerSecond = 20.0f;
    float movePerFrame  = movePerSecond * deltaSeconds;

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
