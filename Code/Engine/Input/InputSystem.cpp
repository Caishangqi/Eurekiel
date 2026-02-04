#include "Engine/Input/InputSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/NamedStrings.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Window/Window.hpp"
#include "Game/GameCommon.hpp"


bool InputSystem::Event_KeyPressed(EventArgs& args)
{
    if (!g_theInput)
    {
        return false;
    }
    unsigned char keyCode = static_cast<unsigned char>(args.GetValue("KeyCode", -1));
    g_theInput->HandleKeyPressed(keyCode);
    return true;
}

bool InputSystem::Event_KeyReleased(EventArgs& args)
{
    if (!g_theInput)
    {
        return false;
    }
    unsigned char keyCode = static_cast<unsigned char>(args.GetValue("KeyCode", -1));
    g_theInput->HandleKeyReleased(keyCode);
    return true;
}

InputSystem::InputSystem(const InputSystemConfig& config)
{
    m_inputConfig = config;
}

InputSystem::~InputSystem()
{
}

void InputSystem::Startup()
{
    for (int controllerID = 0; controllerID < NUM_XBOX_CONTROLLERS; ++controllerID)
    {
        m_controllers[controllerID].m_id = controllerID;
    }
    printf("InputSystem::Startup    Initialize input system\n");
    g_theEventSubsystem->SubscribeStringEvent("KeyPressed", Event_KeyPressed);
    g_theEventSubsystem->SubscribeStringEvent("KeyReleased", Event_KeyReleased);
}

void InputSystem::Shutdown()
{
}

// Immediately use get the button and use in Update()
void InputSystem::BeginFrame()
{
    for (XboxController& m_controller : m_controllers)
    {
        m_controller.Update();
    }


    bool shouldHide = (m_cursorState.m_cursorMode == CursorMode::FPS);
    if (shouldHide != m_isCursorHidden)
    {
        if (shouldHide)
        {
            while (ShowCursor(FALSE) >= 0)
            {
            }
        }
        else
        {
            while (ShowCursor(TRUE) < 0)
            {
            }
        }
        m_isCursorHidden = shouldHide;
    }

    IntVec2 oldCursorPos = m_cursorState.m_cursorClientPosition;

    POINT cursorPos;
    GetCursorPos(&cursorPos);
    auto hWnd = static_cast<HWND>(Window::s_mainWindow->GetWindowHandle());
    ScreenToClient(hWnd, &cursorPos);
    IntVec2 newCursorPos(cursorPos.x, -cursorPos.y);

    if (m_cursorState.m_cursorMode == CursorMode::FPS)
    {
        m_cursorState.m_cursorClientDelta.x = newCursorPos.x - oldCursorPos.x;
        m_cursorState.m_cursorClientDelta.y = newCursorPos.y - oldCursorPos.y;


        IntVec2 clientDims = Window::s_mainWindow->GetClientDimensions();
        POINT   centerPos;
        centerPos.x = clientDims.x / 2;
        centerPos.y = clientDims.y / 2;
        ClientToScreen(hWnd, &centerPos);
        SetCursorPos(centerPos.x, centerPos.y);

        GetCursorPos(&cursorPos);
        ScreenToClient(hWnd, &cursorPos);
        m_cursorState.m_cursorClientPosition = IntVec2(cursorPos.x, -cursorPos.y);
    }
    else
    {
        m_cursorState.m_cursorClientDelta    = IntVec2(0, 0);
        m_cursorState.m_cursorClientPosition = newCursorPos;
    }
}

void InputSystem::EndFrame()
{
    for (int keyIndex = 0; keyIndex < NUM_KEYCODES; ++keyIndex)
    {
        m_keyStates[keyIndex].m_wasPressedLastFrame = m_keyStates[keyIndex].m_isPressed;
    }

    m_mouseWheelDelta = 0; // 重置鼠标滚轮增量
}

void InputSystem::SetCursorMode(CursorMode cursorMode)
{
    m_cursorState.m_cursorMode = cursorMode;
}

CursorMode InputSystem::GetCursorMode() const
{
    return m_cursorState.m_cursorMode;
}

Vec2 InputSystem::GetCursorClientDelta() const
{
    return m_cursorState.m_cursorClientDelta;
}

Vec2 InputSystem::GetCursorClientPosition() const
{
    return Vec2(0, 0);
}

Vec2 InputSystem::GetCursorNormalizedPosition() const
{
    IntVec2 clientDims = Window::s_mainWindow->GetClientDimensions();

    // m_cursorClientPosition.x: [0, clientDims.x] -> [0, 1]
    // m_cursorClientPosition.y: [-clientDims.y, 0] -> [0, 1] (y is negated in BeginFrame)
    float normalizedX = RangeMapClamped(static_cast<float>(m_cursorState.m_cursorClientPosition.x),
                                        0.0f, static_cast<float>(clientDims.x),
                                        0.0f, 1.0f);
    float normalizedY = RangeMapClamped(static_cast<float>(m_cursorState.m_cursorClientPosition.y),
                                        static_cast<float>(-clientDims.y), 0.0f,
                                        0.0f, 1.0f);

    return Vec2(normalizedX, normalizedY);
}

bool InputSystem::WasKeyJustPressed(unsigned char keyCode)
{
    bool isKeyDown        = m_keyStates[keyCode].m_isPressed;
    bool isLastFramePress = m_keyStates[keyCode].m_wasPressedLastFrame;
    return !isKeyDown && isLastFramePress;
}

bool InputSystem::WasKeyJustReleased(unsigned char keyCode)
{
    bool isKeyReleased       = !m_keyStates[keyCode].m_isPressed;
    bool isLastFrameReleased = !m_keyStates[keyCode].m_wasPressedLastFrame;
    return !isKeyReleased && isLastFrameReleased;
}

bool InputSystem::IsKeyDown(unsigned char keyCode)
{
    return m_keyStates[keyCode].m_isPressed;
}

void InputSystem::HandleKeyPressed(unsigned char keyCode)
{
    m_keyStates[keyCode].m_isPressed = true;
}

void InputSystem::HandleKeyReleased(unsigned char keyCode)
{
    m_keyStates[keyCode].m_isPressed = false;
}

const XboxController& InputSystem::GetController(unsigned char controllerIndex)
{
    return m_controllers[controllerIndex];
}

void InputSystem::HandleMouseButtonPressed(unsigned char keyCode)
{
    m_keyStates[keyCode].m_isPressed = true;
}

void InputSystem::HandleMouseButtonReleased(unsigned char keyCode)
{
    m_keyStates[keyCode].m_isPressed = false;
}

void InputSystem::HandleMouseMove(int mouseX, int mouseY)
{
    m_mousePosition = Vec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
}

void InputSystem::HandleMouseWheel(short wheelDelta)
{
    m_mouseWheelDelta = wheelDelta;
}

bool InputSystem::IsMouseButtonDown(unsigned char keyCode) const
{
    return m_keyStates[keyCode].m_isPressed;
}

bool InputSystem::WasMouseButtonJustPressed(unsigned char keyCode) const
{
    bool isKeyDown        = m_keyStates[keyCode].m_isPressed;
    bool isLastFramePress = m_keyStates[keyCode].m_wasPressedLastFrame;
    return !isKeyDown && isLastFramePress;
}

Vec2 InputSystem::GetMousePosition() const
{
    return m_mousePosition;
}

Vec2 InputSystem::GetMousePositionOnWorld(Vec2& cameraBottomLeft, Vec2& cameraTopRight) const
{
    // 获取屏幕上的鼠标位置
    Vec2 mouseScreenPos = GetMousePosition();

    auto windowHandle = static_cast<HWND>(Window::s_mainWindow->GetWindowHandle());

    // 获取窗口客户区域的宽度和高度
    RECT clientRect;
    GetClientRect(windowHandle, &clientRect);

    float clientWidth  = static_cast<float>(clientRect.right - clientRect.left);
    float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);

    // 获取客户区域左上角相对于屏幕的位置
    POINT clientTopLeft = {0, 0};
    ClientToScreen(windowHandle, &clientTopLeft);
    //printf("clientTopLeft: x: %f y: %f\n", static_cast<float>(clientTopLeft.x), static_cast<float>(clientTopLeft.y));

    // 调整鼠标位置以考虑客户区域在屏幕上的位置
    mouseScreenPos.x -= static_cast<float>(clientTopLeft.x);
    mouseScreenPos.y -= static_cast<float>(clientTopLeft.y);

    // 将鼠标屏幕位置映射到世界坐标系
    float worldX = RangeMapClamped(mouseScreenPos.x + static_cast<float>(clientTopLeft.x), 0.0f, clientWidth,
                                   cameraBottomLeft.x, cameraTopRight.x);
    float worldY = RangeMapClamped(mouseScreenPos.y + static_cast<float>(clientTopLeft.y), 0.0f, clientHeight,
                                   cameraTopRight.y, cameraBottomLeft.y);
    return Vec2(worldX, worldY);
}

short InputSystem::GetMouseWheelDelta() const
{
    return m_mouseWheelDelta;
}
