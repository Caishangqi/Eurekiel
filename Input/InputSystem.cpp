#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"


InputSystem::InputSystem()
{
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
    printf("[input]     Initialize input system\n");
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
}

void InputSystem::EndFrame()
{
    for (int keyIndex = 0; keyIndex < NUM_KEYCODES; ++keyIndex)
    {
        m_keyStates[keyIndex].m_wasPressedLastFrame = m_keyStates[keyIndex].m_isPressed;
    }

    for (int keyIndex = 0; keyIndex < 3; ++keyIndex)
    {
        m_buttonStates[keyIndex].m_wasPressedLastFrame = m_buttonStates[keyIndex].m_isPressed;
    }

    m_mouseWheelDelta = 0; // 重置鼠标滚轮增量
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

void InputSystem::HandleMouseButtonPressed(int button)
{
    if (button >= 0 && button < 3)
    {
        m_buttonStates[button].m_isPressed = true;
    }
}

void InputSystem::HandleMouseButtonReleased(int button)
{
    if (button >= 0 && button < 3)
    {
        m_buttonStates[button].m_isPressed = false;
    }
}

void InputSystem::HandleMouseMove(int mouseX, int mouseY)
{
    m_mousePosition = Vec2(static_cast<float>(mouseX), static_cast<float>(mouseY));
}

void InputSystem::HandleMouseWheel(short wheelDelta)
{
    m_mouseWheelDelta = wheelDelta;
}

bool InputSystem::IsMouseButtonDown(int button) const
{
    if (button >= 0 && button < 3)
    {
        return m_buttonStates[button].m_isPressed;
    }
    return false;
}

bool InputSystem::WasMouseButtonJustPressed(int button) const
{
    bool isKeyDown        = m_buttonStates[button].m_isPressed;
    bool isLastFramePress = m_buttonStates[button].m_wasPressedLastFrame;
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

    // 获取窗口客户区域的宽度和高度
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);

    float clientWidth  = static_cast<float>(clientRect.right - clientRect.left);
    float clientHeight = static_cast<float>(clientRect.bottom - clientRect.top);

    // 获取客户区域左上角相对于屏幕的位置
    POINT clientTopLeft = {0, 0};
    ClientToScreen(hWnd, &clientTopLeft);
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
