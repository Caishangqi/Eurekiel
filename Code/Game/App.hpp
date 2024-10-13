#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

class Game;
class ResourceManager;

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


    bool m_isQuitting = false;
    bool m_isPaused   = false;
    bool m_isSlowMo   = false;

    Game*            m_theGame;
    ResourceManager* m_resourceManager;

    float m_LastFrameStartTime;

public:
    bool m_isDebug          = false;
    bool m_isPendingRestart = false;
    HWND hWnd;
};
