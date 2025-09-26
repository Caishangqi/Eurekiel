#include "RendererSubsystem.hpp"

#include "Engine/Core/Logger/LoggerAPI.hpp"
using namespace enigma::graphic;

RendererSubsystem::RendererSubsystem(Configuration& config)
{
    m_configuration = config;
}

RendererSubsystem::~RendererSubsystem()
{
}

void RendererSubsystem::Initialize()
{
    LogInfo(GetStaticSubsystemName(), "Initializing D3D12 rendering system...");

    // 获取窗口句柄（通过配置参数）
    HWND hwnd = nullptr;
    if (m_configuration.targetWindow)
    {
        hwnd = static_cast<HWND>(m_configuration.targetWindow->GetWindowHandle());
        LogInfo(GetStaticSubsystemName(), "Window handle obtained from configuration for SwapChain creation");
    }
    else
    {
        LogWarn(GetStaticSubsystemName(), "No window provided in configuration - initializing in headless mode");
    }

    // 调用D3D12RenderSystem的完整初始化，包括SwapChain创建
    bool success = D3D12RenderSystem::Initialize(
        m_configuration.enableDebugLayer, // Debug layer
        m_configuration.enableGPUValidation, // GPU validation
        hwnd, // Window handle for SwapChain
        m_configuration.renderWidth, // Render width
        m_configuration.renderHeight // Render height
    );

    if (!success)
    {
        LogError(GetStaticSubsystemName(), "Failed to initialize D3D12RenderSystem");
        m_isInitialized = false;
        return;
    }

    m_isInitialized = true;
    LogInfo(GetStaticSubsystemName(), "D3D12RenderSystem initialized successfully through RendererSubsystem");

    // 显示架构流程确认信息
    LogInfo(GetStaticSubsystemName(), "Initialization flow: RendererSubsystem → D3D12RenderSystem → SwapChain creation completed");
}

void RendererSubsystem::Startup()
{
    LogInfo(GetStaticSubsystemName(), "Starting up...");
}

void RendererSubsystem::Shutdown()
{
    LogInfo(GetStaticSubsystemName(), "Shutting down...");
    D3D12RenderSystem::Shutdown();
}

/**
 * @brief 检查渲染系统是否准备好进行渲染
 * @return 如果D3D12RenderSystem已初始化且设备可用则返回true
 *
 * 教学要点: 通过检查底层API封装层的状态确定渲染系统就绪状态
 */
bool RendererSubsystem::IsReadyForRendering() const noexcept
{
    // 通过D3D12RenderSystem静态API检查设备状态
    return D3D12RenderSystem::IsInitialized() && D3D12RenderSystem::GetDevice() != nullptr;
}

void RendererSubsystem::BeginFrame()
{
    // ========================================================================
    // 帧开始处理 - 正确的DirectX 12管线架构
    // ========================================================================

    // 1. 准备下一帧 - 这是每帧开始的必要操作
    // 教学要点：在BeginFrame中统一准备下一帧，保持渲染管线架构清晰
    enigma::graphic::D3D12RenderSystem::PrepareNextFrame();
    LogInfo(GetStaticSubsystemName(), "BeginFrame - Next frame prepared successfully");

    // TODO: 后续扩展
    // - setup1-99: GPU状态初始化、SSBO设置
    // - begin1-99: 每帧参数更新、摄像机矩阵计算
    // - 资源状态转换和绑定
    // - 渲染目标清理

    LogInfo(GetStaticSubsystemName(), "BeginFrame - Frame initialization completed");
}

void RendererSubsystem::Update(float deltaTime)
{
    // TODO: 实现主要渲染逻辑
    // - shadow: 阴影贴图生成
    // - shadowcomp1-99: 阴影后处理
    // - prepare1-99: SSAO等预处理
    // - gbuffers(opaque): 不透明几何体G-Buffer填充
    // - deferred1-99: 延迟光照计算
    // - gbuffers(translucent): 半透明几何体前向渲染
    // - composite1-99: 后处理效果链

    UNUSED(deltaTime)
    //LogInfo(GetStaticSubsystemName(), "Update - deltaTime: " + std::to_string(deltaTime) + "s");
}

void RendererSubsystem::EndFrame()
{
    // ========================================================================
    // 帧结束处理 - 正确的DirectX 12管线架构
    // ========================================================================

    // 1. 回收已完成的命令列表 - 这是每帧必须的维护操作
    // 教学要点：在EndFrame中统一回收，保持渲染管线架构清晰
    auto* commandListManager = enigma::graphic::D3D12RenderSystem::GetCommandListManager();
    if (commandListManager)
    {
        commandListManager->UpdateCompletedCommandLists();
        LogInfo(GetStaticSubsystemName(), "EndFrame - Command lists recycled successfully");
    }
    else
    {
        LogWarn(GetStaticSubsystemName(), "EndFrame - CommandListManager not available");
    }

    // TODO: 后续扩展
    // - final: 最终输出处理
    // - Present: 交换链呈现
    // - 性能统计收集
    // - GPU/CPU同步优化

    LogInfo(GetStaticSubsystemName(), "EndFrame - Frame completed");
}
