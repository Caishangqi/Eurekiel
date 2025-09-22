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
    D3D12RenderSystem::InitializeRenderer(m_debugRenderingEnabled, m_configuration.enableGPUValidation);
}

void RendererSubsystem::Startup()
{
    LogInfo(GetStaticSubsystemName(), "Starting up...");
}

void RendererSubsystem::Shutdown()
{
    LogInfo(GetStaticSubsystemName(), "Shutting down...");
    D3D12RenderSystem::ShutdownRenderer();
}

void RendererSubsystem::BeginFrame()
{
    // TODO: 实现帧开始逻辑
    // - setup1-99: GPU状态初始化、SSBO设置
    // - begin1-99: 每帧参数更新、摄像机矩阵计算
    LogInfo(GetStaticSubsystemName(), "BeginFrame - TODO: Implement frame begin logic");
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
    LogInfo(GetStaticSubsystemName(), "Update - deltaTime: " + std::to_string(deltaTime) + "s");
}

void RendererSubsystem::EndFrame()
{
    // TODO: 实现帧结束逻辑
    // - final: 最终输出处理
    // - Present: 交换链呈现
    // - 性能统计收集
    // - Command List提交和同步
    LogInfo(GetStaticSubsystemName(), "EndFrame - TODO: Implement frame end logic");
}
