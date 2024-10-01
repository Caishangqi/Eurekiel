#pragma once

#include "Game.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Renderer/Renderer.hpp"

class InputSystem;

class App
{
public:
    App();
    ~App();

    void Startup();
    void Shutdown();
    void RunFrame();

    bool IsQuitting() const;
    void HandleQuitRequested();

    void AdjustForPauseAndTimeDistortion(float& deltaSeconds);
    void HandleKeyBoardEvent();

private:
    void BeginFrame();
    void UpdateCameras();
    void Update(float deltaSeconds);
    void Render() const;
    void EndFrame();

    void UpdateShip(float deltaSeconds);
    void RenderShip() const;

    bool m_isQuitting = false;
    bool m_isPaused = false;
    bool m_isSlowMo = false;
    bool m_isPauseAfterFrame = false;
    Vec2 m_shipPos;

    Camera* m_gameCamera;
    // initialize all flase
    bool m_areKeysDown[256] = {}; // true if each key is current down
    bool m_areKeysDownLastFrame[256] = {}; // notes whether key was down last frame
    Game* m_theGame;

    float m_LastFrameStartTime;

public:
    bool m_isDebug = false;
    bool m_isPendingRestart = false;
};
