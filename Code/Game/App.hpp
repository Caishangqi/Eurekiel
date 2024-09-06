#pragma once
#include "Engine/Math/Vec2.hpp"
#include "Engine/Renderer/Renderer.hpp"

class App
{
public:
    App();
    ~App();

    void Startup();
    void Shutdown();
    void RunFrame();

    bool IsQuitting() const;
    bool HandleKeyPress(unsigned char keyCode);
    bool HandleKeyRelease(unsigned char keyCode);
    bool HandleQuitRequested();

private:
    void BeginFrame();
    void Update(float deltaSeconds);
    void Render() const;
    void EndFrame();

    void UpdateShip(float deltaSeconds);
    void RenderShip() const;

    bool m_isPausedAfterFrame = false;
    bool m_isQuitting = false;
    bool m_isPaused = false;
    bool m_isSlowMo = false;
    Vec2 m_shipPos;

    Camera* m_gameCamera;
};
