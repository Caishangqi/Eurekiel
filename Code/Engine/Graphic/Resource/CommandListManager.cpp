#include "Engine/Graphic/Resource/CommandListManager.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include <cassert>
#include <string>

#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
using namespace enigma::graphic;

#pragma region CommandListWrapper
CommandListManager::CommandListWrapper::CommandListWrapper()
{
}

CommandListManager::CommandListWrapper::~CommandListWrapper()
{
    // 智能指针会自动释放DirectX对象
}

/**
 * @brief 移动构造函数
 *
 * 教学要点: 移动语义可以避免不必要的资源复制，提高性能
 */
CommandListManager::CommandListWrapper::CommandListWrapper(CommandListWrapper&& other) noexcept
    : commandList(std::move(other.commandList))
      , commandAllocator(std::move(other.commandAllocator))
      , state(other.state)
      , fenceValue(other.fenceValue)
      , type(other.type)
      , debugName(std::move(other.debugName))
{
    // 重置源对象状态
    other.state      = State::Closed;
    other.fenceValue = 0;
    other.type       = Type::Copy;
}

/**
 * @brief 移动赋值操作符
 */
CommandListManager::CommandListWrapper& CommandListManager::CommandListWrapper::operator=(CommandListWrapper&& other) noexcept
{
    if (this != &other)
    {
        // 移动资源
        commandList      = std::move(other.commandList);
        commandAllocator = std::move(other.commandAllocator);
        state            = other.state;
        fenceValue       = other.fenceValue;
        type             = other.type;
        debugName        = std::move(other.debugName);

        // 重置源对象状态
        other.state      = State::Closed;
        other.fenceValue = 0;
        other.type       = Type::Copy;
    }
    return *this;
}
#pragma endregion


CommandListManager::CommandListManager()
{
}

/**
 * @brief 析构函数 - 确保资源正确清理
 *
 * 教学要点: 析构函数应该调用Shutdown确保所有资源被正确释放
 * 这是RAII (Resource Acquisition Is Initialization) 设计模式的体现
 */
CommandListManager::~CommandListManager()
{
    Shutdown();
}

/**
 * @brief 初始化命令列表管理器
 *
 * 教学要点: DirectX 12的命令列表管理是整个渲染系统的核心
 *
 * 关键概念:
 * 1. 命令队列 (Command Queue): CPU提交命令的入口，GPU按顺序执行
 * 2. 命令分配器 (Command Allocator): 管理命令存储的底层内存
 * 3. 命令列表 (Command List): 录制GPU命令的接口
 * 4. 围栏 (Fence): CPU/GPU同步机制，避免竞态条件
 *
 * Microsoft官方文档:
 * - 命令队列: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/command-queues
 * - 围栏同步: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/user-mode-heap-synchronization
 * - 命令列表: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/recording-command-lists-and-bundles
 */
bool CommandListManager::Initialize(uint32_t graphicsCount, uint32_t computeCount, uint32_t copyCount)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 防止重复初始化
    if (m_initialized)
    {
        return true;
    }

    // 获取D3D12设备 - 从D3D12RenderSystem获取全局设备实例
    // 教学要点: 在DirectX 12中，设备是所有资源和对象的工厂
    auto* device = D3D12RenderSystem::GetDevice();
    if (!device)
    {
        // TODO: 使用统一的日志系统
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(), "× Failed to initialize CommandListManager reason: device is null");
        ERROR_AND_DIE("× Failed to initialize CommandListManager reason: device is null")
    }

    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "CommandListManager::Initialize() - graphicsCount=%u, computeCount=%u, copyCount=%u",
                          graphicsCount, computeCount, copyCount);

    // ========================================================================
    // 第1步: 创建命令队列 (Command Queues)
    // ========================================================================

    /**
     * 教学要点: DirectX 12支持三种命令队列类型
     * - Graphics: 支持所有操作(绘制、计算、复制)
     * - Compute: 仅支持计算和复制操作
     * - Copy: 仅支持复制操作
     *
     * Microsoft文档: https://learn.microsoft.com/zh-cn/windows/win32/api/d3d12/ne-d3d12-d3d12_command_list_type
     */

    // 创建图形命令队列
    D3D12_COMMAND_QUEUE_DESC graphicsQueueDesc = {};
    graphicsQueueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT; // Graphics命令类型
    graphicsQueueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    graphicsQueueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    graphicsQueueDesc.NodeMask                 = 0; // 单GPU系统使用0

    HRESULT hr = device->CreateCommandQueue(&graphicsQueueDesc, IID_PPV_ARGS(&m_graphicsQueue));
    if (FAILED(hr))
    {
        // TODO: 错误日志 - 图形命令队列创建失败
        return false;
    }

    // 设置调试名称 - 便于PIX和Visual Studio Graphics Debugger调试
    // Microsoft文档: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/using-d3d12-debug-layer
    m_graphicsQueue->SetName(L"Enigma Graphics Command Queue");

    // 创建计算命令队列
    D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
    computeQueueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_COMPUTE; // Compute命令类型
    computeQueueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    computeQueueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    computeQueueDesc.NodeMask                 = 0;

    hr = device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&m_computeQueue));
    if (FAILED(hr))
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(), "Fail to create Compute Queue, Abort Program");
        ERROR_AND_DIE("Fail to create Compute Queue, Abort Program")
    }
    m_computeQueue->SetName(L"Enigma Compute Command Queue");

    // 创建复制命令队列
    D3D12_COMMAND_QUEUE_DESC copyQueueDesc = {};
    copyQueueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_COPY; // Copy命令类型
    copyQueueDesc.Priority                 = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    copyQueueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
    copyQueueDesc.NodeMask                 = 0;

    hr = device->CreateCommandQueue(&copyQueueDesc, IID_PPV_ARGS(&m_copyQueue));
    if (FAILED(hr))
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(), "Fail to create Copy Command Queue, Abort Program");
        ERROR_AND_DIE("Fail to create Copy Command Queue, Abort Program")
    }
    m_copyQueue->SetName(L"Enigma Copy Command Queue");

    // ========================================================================
    // 第2步: 创建围栏对象 (Fence) - CPU/GPU同步核心
    // ========================================================================

    /**
     * 教学要点: 围栏是DirectX 12中CPU和GPU同步的关键机制
     *
     * 工作原理:
     * 1. CPU提交命令到队列，同时在围栏上设置一个值 (Signal)
     * 2. GPU执行完命令后，将围栏设置为该值
     * 3. CPU可以查询围栏当前值，判断GPU是否完成了特定批次的命令
     *
     * ⭐ Milestone 2.8 架构修复: 每个命令队列使用独立的Fence对象
     * Microsoft最佳实践: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/user-mode-heap-synchronization
     *
     * 为什么需要三个独立Fence:
     * - 避免多队列共享Fence导致的竞态条件
     * - Graphics/Compute/Copy队列在硬件层面并行执行
     * - 共享Fence会导致错误的命令列表回收时机
     */

    // 创建Graphics队列的Fence
    m_graphicsFenceValue = 0;
    hr                   = device->CreateFence(m_graphicsFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_graphicsFence));
    if (FAILED(hr))
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(), "Fail to create Graphics Fence, Abort Program");
        ERROR_AND_DIE("Fail to create Graphics Fence, Abort Program")
    }
    m_graphicsFence->SetName(L"Enigma Graphics Queue Fence");

    // 创建Compute队列的Fence
    m_computeFenceValue = 0;
    hr                  = device->CreateFence(m_computeFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_computeFence));
    if (FAILED(hr))
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(), "Fail to create Compute Fence, Abort Program");
        ERROR_AND_DIE("Fail to create Compute Fence, Abort Program")
    }
    m_computeFence->SetName(L"Enigma Compute Queue Fence");

    // 创建Copy队列的Fence ⭐ 核心修复
    m_copyFenceValue = 0;
    hr               = device->CreateFence(m_copyFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_copyFence));
    if (FAILED(hr))
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(), "Fail to create Copy Fence, Abort Program");
        ERROR_AND_DIE("Fail to create Copy Fence, Abort Program")
    }
    m_copyFence->SetName(L"Enigma Copy Queue Fence");

    // 创建围栏事件 - 用于CPU等待GPU完成
    // 教学要点: Win32事件对象用于阻塞CPU线程直到GPU完成特定操作
    m_fenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
    if (m_fenceEvent == nullptr)
    {
        LogError(RendererSubsystem::GetStaticSubsystemName(), "Fail to create Fence Event, Abort Program");
        ERROR_AND_DIE("Fail to create Fence Event, Abort Program")
    }

    // ========================================================================
    // 第3步: 创建命令列表池 (Command List Pools)
    // ========================================================================

    /**
     * 教学要点: 命令列表池化技术的优势
     *
     * 为什么需要池化:
     * 1. 避免频繁创建/销毁DirectX对象的开销
     * 2. 支持多线程并行录制不同的命令列表
     * 3. 实现命令列表的复用，减少内存分配
     * 4. 简化同步管理，每个池独立管理生命周期
     *
     * Microsoft最佳实践: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/multi-engine
     */

    bool success = true;
    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(), "Creating command list pools...");
    success &= CreateCommandListPool(Type::Graphics, graphicsCount);
    success &= CreateCommandListPool(Type::Compute, computeCount);
    success &= CreateCommandListPool(Type::Copy, copyCount);

    if (!success)
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(), "× Failed to create command list pools");
        // 清理部分创建的资源
        Shutdown();
        return false;
    }

    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "+ All command list pools created successfully - Graphics:%u, Compute:%u, Copy:%u",
                          graphicsCount, computeCount, copyCount);

    m_initialized = true;
    LogInfo(RendererSubsystem::GetStaticSubsystemName(), "+ CommandListManager Initialized");
    return true;
}

/**
 * @brief 释放所有资源 - RAII资源管理
 *
 * 教学要点: DirectX 12资源清理的正确顺序很重要
 *
 * 清理顺序:
 * 1. 等待所有GPU操作完成
 * 2. 清理命令列表和分配器 (智能指针自动管理)
 * 3. 清理同步对象 (围栏和事件)
 * 4. 清理命令队列
 *
 * Microsoft文档: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/fence-based-resource-management
 */
void CommandListManager::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
    {
        return;
    }

    // 第1步: 等待所有GPU操作完成 - 确保没有资源正在使用中
    // 教学要点: 在释放任何DirectX资源之前，必须确保GPU已完成所有相关操作
    // ⭐ Milestone 2.8: 等待所有三个队列完成
    if (m_fenceEvent)
    {
        // 等待Graphics队列
        if (m_graphicsQueue && m_graphicsFence)
        {
            ++m_graphicsFenceValue;
            m_graphicsQueue->Signal(m_graphicsFence.Get(), m_graphicsFenceValue);
            if (m_graphicsFence->GetCompletedValue() < m_graphicsFenceValue)
            {
                m_graphicsFence->SetEventOnCompletion(m_graphicsFenceValue, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, 5000);
            }
        }

        // 等待Compute队列
        if (m_computeQueue && m_computeFence)
        {
            ++m_computeFenceValue;
            m_computeQueue->Signal(m_computeFence.Get(), m_computeFenceValue);
            if (m_computeFence->GetCompletedValue() < m_computeFenceValue)
            {
                m_computeFence->SetEventOnCompletion(m_computeFenceValue, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, 5000);
            }
        }

        // 等待Copy队列
        if (m_copyQueue && m_copyFence)
        {
            ++m_copyFenceValue;
            m_copyQueue->Signal(m_copyFence.Get(), m_copyFenceValue);
            if (m_copyFence->GetCompletedValue() < m_copyFenceValue)
            {
                m_copyFence->SetEventOnCompletion(m_copyFenceValue, m_fenceEvent);
                WaitForSingleObject(m_fenceEvent, 5000);
            }
        }
    }

    // 第2步: 清理命令列表池 - 智能指针会自动释放DirectX对象
    // 教学要点: 使用智能指针的好处 - 自动管理资源生命周期，避免内存泄漏

    // 清空队列 (队列中只是指针，不拥有对象)
    while (!m_availableGraphicsLists.empty()) m_availableGraphicsLists.pop();
    while (!m_availableComputeLists.empty()) m_availableComputeLists.pop();
    while (!m_availableCopyLists.empty()) m_availableCopyLists.pop();

    m_executingLists.clear();

    // 清空容器 - unique_ptr会自动调用CommandListWrapper析构函数
    m_graphicsCommandLists.clear();
    m_computeCommandLists.clear();
    m_copyCommandLists.clear();

    // 第3步: 清理同步对象
    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }

    // 智能指针会自动释放围栏对象 (Milestone 2.8: 清理三个Fence)
    m_graphicsFence.Reset();
    m_graphicsFenceValue = 0;

    m_computeFence.Reset();
    m_computeFenceValue = 0;

    m_copyFence.Reset();
    m_copyFenceValue = 0;

    // 第4步: 清理命令队列 - 智能指针自动管理
    m_graphicsQueue.Reset();
    m_computeQueue.Reset();
    m_copyQueue.Reset();

    m_initialized = false;
}

/**
 * @brief 等待GPU完成所有命令
 *
 * ⭐ Milestone 2.8: 等待所有三个队列的Fence完成
 */
bool CommandListManager::WaitForGPU(uint32_t timeoutMs)
{
    if (!m_fenceEvent)
    {
        return false;
    }

    // 等待Graphics队列
    if (m_graphicsFence && m_graphicsFenceValue > 0)
    {
        if (m_graphicsFence->GetCompletedValue() < m_graphicsFenceValue)
        {
            HRESULT hr = m_graphicsFence->SetEventOnCompletion(m_graphicsFenceValue, m_fenceEvent);
            if (FAILED(hr)) return false;

            DWORD waitResult = WaitForSingleObject(m_fenceEvent, timeoutMs);
            if (waitResult != WAIT_OBJECT_0) return false;
        }
    }

    // 等待Compute队列
    if (m_computeFence && m_computeFenceValue > 0)
    {
        if (m_computeFence->GetCompletedValue() < m_computeFenceValue)
        {
            HRESULT hr = m_computeFence->SetEventOnCompletion(m_computeFenceValue, m_fenceEvent);
            if (FAILED(hr)) return false;

            DWORD waitResult = WaitForSingleObject(m_fenceEvent, timeoutMs);
            if (waitResult != WAIT_OBJECT_0) return false;
        }
    }

    // 等待Copy队列
    if (m_copyFence && m_copyFenceValue > 0)
    {
        if (m_copyFence->GetCompletedValue() < m_copyFenceValue)
        {
            HRESULT hr = m_copyFence->SetEventOnCompletion(m_copyFenceValue, m_fenceEvent);
            if (FAILED(hr)) return false;

            DWORD waitResult = WaitForSingleObject(m_fenceEvent, timeoutMs);
            if (waitResult != WAIT_OBJECT_0) return false;
        }
    }

    return true;
}

ID3D12CommandQueue* CommandListManager::GetCommandQueue(Type type) const
{
    switch (type)
    {
    case Type::Graphics: return m_graphicsQueue.Get();
    case Type::Compute: return m_computeQueue.Get();
    case Type::Copy: return m_copyQueue.Get();
    default: return nullptr;
    }
}

// ========================================================================
// 私有方法实现 - 命令列表池管理
// ========================================================================

/**
 * @brief 创建命令列表池
 *
 * 教学要点: 每个命令列表都需要配对的命令分配器
 *
 * Microsoft文档: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/recording-command-lists-and-bundles
 */
bool CommandListManager::CreateCommandListPool(Type type, uint32_t count)
{
    const char* typeName = GetTypeName(type);
    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "Creating %s command list pool with %u command lists...", typeName, count);

    auto* device = D3D12RenderSystem::GetDevice();
    if (!device || count == 0)
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "× CreateCommandListPool failed - device=%p, count=%u", device, count);
        return false;
    }

    // 获取对应类型的容器引用
    std::vector<std::unique_ptr<CommandListWrapper>>* container      = nullptr;
    std::queue<CommandListWrapper*>*                  availableQueue = nullptr;

    switch (type)
    {
    case Type::Graphics:
        container = &m_graphicsCommandLists;
        availableQueue = &m_availableGraphicsLists;
        break;
    case Type::Compute:
        container = &m_computeCommandLists;
        availableQueue = &m_availableComputeLists;
        break;
    case Type::Copy:
        container = &m_copyCommandLists;
        availableQueue = &m_availableCopyLists;
        break;
    default:
        return false;
    }

    // 预分配容器空间
    container->reserve(count);

    // 创建指定数量的命令列表
    for (uint32_t i = 0; i < count; ++i)
    {
        auto wrapper = CreateCommandList(type);
        if (!wrapper)
        {
            enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                                   "× Failed to create %s command list %u/%u", typeName, i + 1, count);
            return false;
        }

        // 添加到可用队列
        availableQueue->push(wrapper.get());

        // 移动到容器中
        container->push_back(std::move(wrapper));
    }

    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "+ Successfully created %u %s command lists", count, typeName);
    return true;
}

/**
 * @brief 创建单个命令列表
 *
 * 教学要点: DirectX 12的命令列表需要配套的命令分配器
 *
 * 关键概念:
 * 1. 命令分配器负责管理命令的底层内存存储
 * 2. 多个命令列表可以共享同一个分配器(但不能同时录制)
 * 3. 为简化管理，我们为每个命令列表配备独立的分配器
 *
 * Microsoft文档: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/recording-command-lists-and-bundles
 */
std::unique_ptr<CommandListManager::CommandListWrapper> CommandListManager::CreateCommandList(Type type)
{
    auto* device = D3D12RenderSystem::GetDevice();
    if (!device)
    {
        return nullptr;
    }

    auto wrapper  = std::make_unique<CommandListWrapper>();
    wrapper->type = type;

    // 获取DirectX对应的命令列表类型
    D3D12_COMMAND_LIST_TYPE d3d12Type;
    switch (type)
    {
    case Type::Graphics: d3d12Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        break;
    case Type::Compute: d3d12Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
        break;
    case Type::Copy: d3d12Type = D3D12_COMMAND_LIST_TYPE_COPY;
        break;
    default: return nullptr;
    }

    // 创建命令分配器 - 管理命令存储的底层内存
    // 教学要点: 每个命令列表需要独立的分配器，避免多线程竞态
    HRESULT hr = device->CreateCommandAllocator(
        d3d12Type,
        IID_PPV_ARGS(&wrapper->commandAllocator)
    );

    if (FAILED(hr))
    {
        // TODO: 错误日志 - 命令分配器创建失败
        return nullptr;
    }

    // 创建命令列表
    hr = device->CreateCommandList(
        0, // NodeMask - 单GPU系统使用0
        d3d12Type, // 命令列表类型
        wrapper->commandAllocator.Get(), // 关联的命令分配器
        nullptr, // 初始管线状态 (可以为nullptr)
        IID_PPV_ARGS(&wrapper->commandList)
    );

    if (FAILED(hr))
    {
        // TODO: 错误日志 - 命令列表创建失败
        return nullptr;
    }

    // DirectX 12创建的命令列表默认是Recording状态
    // 我们需要关闭它，使其处于可提交状态
    hr = wrapper->commandList->Close();
    if (FAILED(hr))
    {
        // TODO: 错误日志 - 命令列表关闭失败
        return nullptr;
    }

    wrapper->state = State::Closed;

    // 设置调试名称
    std::wstring debugName = L"Enigma Command List - ";
    const char*  typeName  = GetTypeName(type);
    if (typeName)
    {
        // 将const char*转换为std::wstring
        std::string typeNameStr(typeName);
        debugName += std::wstring(typeNameStr.begin(), typeNameStr.end());
    }
    else
    {
        debugName += L"Unknown";
    }

    wrapper->commandList->SetName(debugName.c_str());
    wrapper->commandAllocator->SetName((debugName + L" Allocator").c_str());

    return wrapper;
}

/**
 * @brief 获取指定类型的空闲队列引用
 */
std::queue<CommandListManager::CommandListWrapper*>& CommandListManager::GetAvailableQueue(Type type)
{
    switch (type)
    {
    case Type::Graphics: return m_availableGraphicsLists;
    case Type::Compute: return m_availableComputeLists;
    case Type::Copy: return m_availableCopyLists;
    default:
        // 这应该不会发生，但为了安全返回一个引用
        static std::queue<CommandListWrapper*> dummy;
        return dummy;
    }
}

/**
 * @brief 获取命令列表类型的字符串名称 (调试用)
 */
const char* CommandListManager::GetTypeName(Type type)
{
    switch (type)
    {
    case Type::Graphics: return "Graphics";
    case Type::Compute: return "Compute";
    case Type::Copy: return "Copy";
    default: return "Unknown";
    }
}

// ========================================================================
// 核心使用接口实现 - 命令列表获取和执行
// ========================================================================

/**
 * @brief 获取可用的命令列表开始录制
 *
 * 教学要点: DirectX 12的命令录制模式
 *
 * 关键步骤:
 * 1. 从空闲队列中获取命令列表
 * 2. 重置命令分配器 (清空之前的命令)
 * 3. 重置命令列表 (准备录制新命令)
 * 4. 设置调试名称便于调试
 *
 * Microsoft文档: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/recording-command-lists-and-bundles
 */
ID3D12GraphicsCommandList* CommandListManager::AcquireCommandList(Type type, const std::string& debugName)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const char* typeName = GetTypeName(type);
    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "AcquireCommandList() - type=%s, debugName='%s'", typeName, debugName.c_str());

    if (!m_initialized)
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "- AcquireCommandList failed - CommandListManager not initialized");
        return nullptr;
    }

    // 注意：命令列表回收现在在EndFrame阶段统一处理，保持架构清晰

    // 获取对应类型的空闲队列
    auto& availableQueue = GetAvailableQueue(type);
    if (availableQueue.empty())
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "- AcquireCommandList failed - no available %s command lists in queue", typeName);
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "   This indicates the command list pool was not properly created");
        return nullptr;
    }

    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "Found %zu available %s command lists in queue", availableQueue.size(), typeName);

    // 从队列中取出一个可用的命令列表
    CommandListWrapper* wrapper = availableQueue.front();
    availableQueue.pop();

    assert(wrapper != nullptr);
    assert(wrapper->state == State::Closed);

    // 重置命令分配器 - 清空之前录制的所有命令
    // 教学要点: 必须确保GPU已完成使用此分配器的所有命令
    HRESULT hr = wrapper->commandAllocator->Reset();
    if (FAILED(hr))
    {
        // TODO: 错误日志 - 命令分配器重置失败
        // 将命令列表放回队列
        availableQueue.push(wrapper);
        return nullptr;
    }

    // 重置命令列表 - 准备录制新的命令
    // 教学要点: Reset使命令列表进入Recording状态
    hr = wrapper->commandList->Reset(wrapper->commandAllocator.Get(), nullptr);
    if (FAILED(hr))
    {
        // TODO: 错误日志 - 命令列表重置失败
        availableQueue.push(wrapper);
        return nullptr;
    }

    // 更新状态
    wrapper->state      = State::Recording;
    wrapper->fenceValue = 0; // 将在ExecuteCommandList中设置

    // 设置调试名称 (如果提供)
    if (!debugName.empty())
    {
        wrapper->debugName = debugName;
        std::wstring wDebugName(debugName.begin(), debugName.end());
        wrapper->commandList->SetName(wDebugName.c_str());
    }

    return wrapper->commandList.Get();
}

/**
 * @brief 提交命令列表执行
 *
 * 教学要点: DirectX 12的命令提交和同步机制
 *
 * 关键步骤:
 * 1. 关闭命令列表 (结束录制)
 * 2. 提交到对应的命令队列
 * 3. 发送围栏信号标记完成时间点
 * 4. 添加到执行中列表等待GPU完成
 *
 * Microsoft文档: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/executing-and-synchronizing-command-lists
 */
uint64_t CommandListManager::ExecuteCommandList(ID3D12GraphicsCommandList* commandList)
{
    if (!commandList || !m_initialized)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 查找对应的包装器
    CommandListWrapper* wrapper = FindWrapper(commandList);
    if (!wrapper || wrapper->state != State::Recording)
    {
        // TODO: 错误日志 - 找不到对应的包装器或状态不正确
        return 0;
    }

    // 关闭命令列表 - 结束录制阶段
    HRESULT hr = commandList->Close();
    if (FAILED(hr))
    {
        // TODO: 错误日志 - 命令列表关闭失败
        return 0;
    }

    wrapper->state = State::Closed;

    // 获取对应的命令队列
    ID3D12CommandQueue* queue = GetCommandQueue(wrapper->type);
    if (!queue)
    {
        // TODO: 错误日志 - 找不到对应的命令队列
        return 0;
    }

    // 提交命令列表到GPU执行
    ID3D12CommandList* commandLists[] = {commandList};
    queue->ExecuteCommandLists(1, commandLists);

    // ⭐ Milestone 2.8: 根据Type选择对应的Fence对象和Fence值
    // 教学要点: 每个队列使用独立的Fence避免竞态条件
    uint64_t&    fenceValue = GetFenceValueByType(wrapper->type);
    ID3D12Fence* fence      = GetFenceByType(wrapper->type);

    if (!fence)
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "ExecuteCommandList() - Failed to get Fence for type %s",
                               GetTypeName(wrapper->type));
        return 0;
    }

    // 增加围栏值并发送信号 - 标记这批命令的完成时机
    ++fenceValue;
    hr = queue->Signal(fence, fenceValue);
    if (FAILED(hr))
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "ExecuteCommandList() - Signal failed for %s queue",
                               GetTypeName(wrapper->type));
        return 0;
    }

    // 更新包装器状态
    wrapper->state      = State::Executing;
    wrapper->fenceValue = fenceValue;

    // 添加到执行中列表
    m_executingLists.push_back(wrapper);

    return fenceValue; // ⭐ Milestone 2.8: 返回对应队列的Fence值
}

/**
 * @brief 批量提交多个命令列表
 *
 * 教学要点: 批量提交可以减少API调用开销
 *
 * ⭐ Milestone 2.8: 重构为支持三个独立Fence
 * - 验证所有命令列表为同一类型
 * - 使用对应类型的Fence和队列
 */
uint64_t CommandListManager::ExecuteCommandLists(ID3D12GraphicsCommandList* const* commandLists, uint32_t count)
{
    if (!commandLists || count == 0 || !m_initialized)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 验证所有命令列表并关闭它们
    std::vector<CommandListWrapper*> wrappers;
    wrappers.reserve(count);

    Type batchType    = Type::Graphics; // 批次类型，所有命令列表必须相同
    bool firstWrapper = true;

    for (uint32_t i = 0; i < count; ++i)
    {
        CommandListWrapper* wrapper = FindWrapper(commandLists[i]);
        if (!wrapper || wrapper->state != State::Recording)
        {
            enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                                   "ExecuteCommandLists() - Invalid wrapper or state for command list %u", i);
            return 0;
        }

        // ⭐ Milestone 2.8: 验证所有命令列表类型一致
        if (firstWrapper)
        {
            batchType    = wrapper->type;
            firstWrapper = false;
        }
        else if (wrapper->type != batchType)
        {
            enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                                   "ExecuteCommandLists() - Mixed types detected! Expected %s, got %s at index %u",
                                   GetTypeName(batchType), GetTypeName(wrapper->type), i);
            return 0;
        }

        HRESULT hr = commandLists[i]->Close();
        if (FAILED(hr))
        {
            enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                                   "ExecuteCommandLists() - Failed to close command list %u", i);
            return 0;
        }

        wrapper->state = State::Closed;
        wrappers.push_back(wrapper);
    }

    // ⭐ Milestone 2.8: 获取对应类型的命令队列
    ID3D12CommandQueue* queue = GetCommandQueue(batchType);
    if (!queue)
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "ExecuteCommandLists() - Failed to get queue for type %s",
                               GetTypeName(batchType));
        return 0;
    }

    // 批量提交
    queue->ExecuteCommandLists(count, reinterpret_cast<ID3D12CommandList* const*>(commandLists));

    // ⭐ Milestone 2.8: 根据Type选择对应的Fence对象和Fence值
    uint64_t&    fenceValue = GetFenceValueByType(batchType);
    ID3D12Fence* fence      = GetFenceByType(batchType);

    if (!fence)
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "ExecuteCommandLists() - Failed to get Fence for type %s",
                               GetTypeName(batchType));
        return 0;
    }

    // 增加围栏值并发送信号
    ++fenceValue;
    HRESULT hr = queue->Signal(fence, fenceValue);
    if (FAILED(hr))
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "ExecuteCommandLists() - Signal failed for %s queue",
                               GetTypeName(batchType));
        return 0;
    }

    // 更新所有包装器状态
    for (CommandListWrapper* wrapper : wrappers)
    {
        wrapper->state      = State::Executing;
        wrapper->fenceValue = fenceValue;
        m_executingLists.push_back(wrapper);
    }

    return fenceValue; // ⭐ Milestone 2.8: 返回对应队列的Fence值
}

// ========================================================================
// 同步管理接口实现
// ========================================================================

/**
 * @brief 等待特定围栏值完成
 *
 * 教学要点: CPU等待GPU完成特定命令批次的机制
 *
 * ⭐ Milestone 2.8: 智能Fence选择 - 根据fenceValue查找对应的Fence对象
 *
 * Microsoft文档: https://learn.microsoft.com/zh-cn/windows/win32/direct3d12/user-mode-heap-synchronization
 */
bool CommandListManager::WaitForFence(uint64_t fenceValue, uint32_t timeoutMs)
{
    if (!m_fenceEvent)
    {
        return false;
    }

    // ⭐ Milestone 2.8: 根据fenceValue智能查找对应的Fence对象
    // 教学要点: 通过wrapper->type确定应该等待哪个Fence
    CommandListWrapper* wrapper = FindWrapperByFenceValue(fenceValue);

    ID3D12Fence* fence = nullptr;
    if (wrapper)
    {
        // 找到了对应的wrapper，使用其type获取Fence
        fence = GetFenceByType(wrapper->type);
    }
    else
    {
        // 未找到wrapper，可能已经完成，尝试检查所有三个Fence
        // 这是一个fallback机制，确保等待能够完成
        // 优先检查Graphics（最常用）
        if (m_graphicsFence && m_graphicsFence->GetCompletedValue() >= fenceValue)
        {
            return true;
        }
        if (m_computeFence && m_computeFence->GetCompletedValue() >= fenceValue)
        {
            return true;
        }
        if (m_copyFence && m_copyFence->GetCompletedValue() >= fenceValue)
        {
            return true;
        }

        // 未找到匹配的Fence，使用Graphics Fence作为默认（兼容性处理）
        fence = m_graphicsFence.Get();
        enigma::core::LogWarn(RendererSubsystem::GetStaticSubsystemName(),
                              "WaitForFence() - Could not find wrapper for fenceValue %llu, using Graphics Fence",
                              fenceValue);
    }

    if (!fence)
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "WaitForFence() - Fence is null");
        return false;
    }

    // 检查围栏是否已经完成
    if (fence->GetCompletedValue() >= fenceValue)
    {
        return true; // 已经完成，无需等待
    }

    // 设置事件，当围栏到达指定值时触发
    HRESULT hr = fence->SetEventOnCompletion(fenceValue, m_fenceEvent);
    if (FAILED(hr))
    {
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "WaitForFence() - SetEventOnCompletion failed");
        return false;
    }

    // 等待事件触发
    DWORD waitResult = WaitForSingleObject(m_fenceEvent, timeoutMs);
    return waitResult == WAIT_OBJECT_0;
}

// WaitForGPU方法已在第313行定义，删除重复定义

/**
 * @brief 检查围栏值是否已完成
 *
 * ⭐ Milestone 2.8: 智能Fence选择 - 根据fenceValue查找对应的Fence对象
 *
 * 教学要点: 与WaitForFence()类似,需要智能查找对应的Fence
 */
bool CommandListManager::IsFenceCompleted(uint64_t fenceValue) const
{
    // ⭐ Milestone 2.8: 根据fenceValue智能查找对应的Fence对象
    // 注意: 这里需要通过const_cast访问非const方法FindWrapperByFenceValue()
    CommandListWrapper* wrapper = const_cast<CommandListManager*>(this)->FindWrapperByFenceValue(fenceValue);

    ID3D12Fence* fence = nullptr;
    if (wrapper)
    {
        // 找到了对应的wrapper，使用其type获取Fence
        fence = const_cast<CommandListManager*>(this)->GetFenceByType(wrapper->type);
    }
    else
    {
        // 未找到wrapper，可能已经完成，尝试检查所有三个Fence
        // 这是一个fallback机制
        if (m_graphicsFence && m_graphicsFence->GetCompletedValue() >= fenceValue)
        {
            return true;
        }
        if (m_computeFence && m_computeFence->GetCompletedValue() >= fenceValue)
        {
            return true;
        }
        if (m_copyFence && m_copyFence->GetCompletedValue() >= fenceValue)
        {
            return true;
        }

        // 未找到任何匹配的Fence
        return false;
    }

    if (!fence)
    {
        return false;
    }

    return fence->GetCompletedValue() >= fenceValue;
}

/**
 * @brief 获取当前已完成的围栏值
 *
 * ⭐ Milestone 2.8: 返回三个Fence中的最小完成值 - 最保守的完成值
 *
 * 教学要点: 返回最小值确保这是"所有队列都完成"的安全值
 * - 如果返回最大值,可能某些队列还未完成
 * - 返回最小值是最保守的策略,确保跨队列同步安全
 */
uint64_t CommandListManager::GetCompletedFenceValue() const
{
    uint64_t minCompletedValue = UINT64_MAX;

    // 获取Graphics队列的完成值
    if (m_graphicsFence)
    {
        uint64_t graphicsCompleted = m_graphicsFence->GetCompletedValue();
        minCompletedValue          = (std::min)(minCompletedValue, graphicsCompleted);
    }

    // 获取Compute队列的完成值
    if (m_computeFence)
    {
        uint64_t computeCompleted = m_computeFence->GetCompletedValue();
        minCompletedValue         = (std::min)(minCompletedValue, computeCompleted);
    }

    // 获取Copy队列的完成值
    if (m_copyFence)
    {
        uint64_t copyCompleted = m_copyFence->GetCompletedValue();
        minCompletedValue      = (std::min)(minCompletedValue, copyCompleted);
    }

    // 如果所有Fence都为null,返回0
    if (minCompletedValue == UINT64_MAX)
    {
        return 0;
    }

    return minCompletedValue;
}

// ========================================================================
// 资源管理和维护接口实现
// ========================================================================

/**
 * @brief 回收已完成的命令列表
 *
 * 教学要点:
 * 1. 每帧在EndFrame中调用一次，确保资源及时回收
 * 2. 线程安全设计，支持多线程环境
 * 3. 这是DirectX 12资源池化的核心机制
 *
 * ⭐ Milestone 2.8: 检查每个wrapper对应的Fence - 三个独立Fence架构
 */
void CommandListManager::UpdateCompletedCommandLists()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 🔍 DEBUG: 记录调用
    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "UpdateCompletedCommandLists() called - ExecutingCount=%zu",
                          m_executingLists.size());

    if (m_executingLists.empty())
    {
        enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                              "UpdateCompletedCommandLists() - No executing lists, skipping");
        return;
    }

    size_t recycledCount = 0;

    // ⭐ Milestone 2.8: 显示所有三个Fence的状态
    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "GPU Fence Status - Graphics: %llu/%llu, Compute: %llu/%llu, Copy: %llu/%llu",
                          m_graphicsFence ? m_graphicsFence->GetCompletedValue() : 0, m_graphicsFenceValue,
                          m_computeFence ? m_computeFence->GetCompletedValue() : 0, m_computeFenceValue,
                          m_copyFence ? m_copyFence->GetCompletedValue() : 0, m_copyFenceValue);

    // 检查执行中的命令列表，将完成的移回空闲队列
    auto it = m_executingLists.begin();
    while (it != m_executingLists.end())
    {
        CommandListWrapper* wrapper = *it;

        // ⭐ Milestone 2.8: 根据wrapper->type获取对应的Fence
        ID3D12Fence* fence = GetFenceByType(wrapper->type);
        if (!fence)
        {
            enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                                   "UpdateCompletedCommandLists() - Fence is null for type %s",
                                   GetTypeName(wrapper->type));
            ++it;
            continue;
        }

        uint64_t completedValue = fence->GetCompletedValue();

        // 🔍 DEBUG: 显示每个命令列表的围栏值
        enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                              "  Checking wrapper[%s]: fenceValue=%llu (completed=%llu, canRecycle=%s)",
                              GetTypeName(wrapper->type),
                              wrapper->fenceValue,
                              completedValue,
                              (wrapper->fenceValue <= completedValue) ? "YES" : "NO");

        if (wrapper->fenceValue <= completedValue)
        {
            // GPU已完成此命令列表，可以回收复用
            // 教学要点：将状态设置为Closed，表示可以被重新使用
            wrapper->state = State::Closed;

            // 添加到对应的空闲队列
            auto& availableQueue = GetAvailableQueue(wrapper->type);
            availableQueue.push(wrapper);

            enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                                  "  + Recycled %s command list (fenceValue=%llu)",
                                  GetTypeName(wrapper->type), wrapper->fenceValue);

            // 从执行中列表移除
            it = m_executingLists.erase(it);
            recycledCount++;
        }
        else
        {
            enigma::core::LogWarn(RendererSubsystem::GetStaticSubsystemName(),
                                  "  ! Cannot recycle %s command list - fenceValue(%llu) > completedValue(%llu)",
                                  GetTypeName(wrapper->type), wrapper->fenceValue, completedValue);
            ++it;
        }
    }

    enigma::core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                          "UpdateCompletedCommandLists() finished - Recycled=%zu, Remaining=%zu",
                          recycledCount, m_executingLists.size());
}

/**
 * @brief 强制回收所有命令列表 (等待GPU完成)
 */
void CommandListManager::FlushAllCommandLists()
{
    if (!m_initialized)
    {
        return;
    }

    // 等待所有命令完成
    WaitForGPU();

    // 回收所有命令列表
    UpdateCompletedCommandLists();
}

/**
 * @brief 获取指定类型的可用命令列表数量
 */
uint32_t CommandListManager::GetAvailableCount(Type type) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto& availableQueue = const_cast<CommandListManager*>(this)->GetAvailableQueue(type);
    return static_cast<uint32_t>(availableQueue.size());
}

/**
 * @brief 获取指定类型的执行中命令列表数量
 */
uint32_t CommandListManager::GetExecutingCount(Type type) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t count = 0;
    for (const auto* wrapper : m_executingLists)
    {
        if (wrapper->type == type)
        {
            ++count;
        }
    }

    return count;
}

/**
 * @brief 查找命令列表包装器
 */
CommandListManager::CommandListWrapper* CommandListManager::FindWrapper(ID3D12GraphicsCommandList* commandList)
{
    if (!commandList)
    {
        return nullptr;
    }

    // 在所有池中查找匹配的命令列表
    auto searchInPool = [commandList](const std::vector<std::unique_ptr<CommandListWrapper>>& pool) -> CommandListWrapper*
    {
        for (const auto& wrapper : pool)
        {
            if (wrapper->commandList.Get() == commandList)
            {
                return wrapper.get();
            }
        }
        return nullptr;
    };

    // 按照使用频率顺序搜索
    CommandListWrapper* found = nullptr;

    found = searchInPool(m_graphicsCommandLists);
    if (found) return found;

    found = searchInPool(m_computeCommandLists);
    if (found) return found;

    found = searchInPool(m_copyCommandLists);
    if (found) return found;

    return nullptr;
}

// ========================================================================
// Fence辅助方法实现 (Milestone 2.8)
// ========================================================================

/**
 * @brief 根据fenceValue查找对应的CommandListWrapper
 *
 * 教学要点: 用于WaitForFence()智能选择正确的Fence对象
 * - 遍历执行中的命令列表
 * - 根据fenceValue匹配找到对应的wrapper
 * - 返回wrapper后可以通过wrapper->type获取命令列表类型
 */
CommandListManager::CommandListWrapper* CommandListManager::FindWrapperByFenceValue(uint64_t fenceValue)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto* wrapper : m_executingLists)
    {
        if (wrapper && wrapper->fenceValue == fenceValue)
        {
            return wrapper;
        }
    }

    return nullptr;
}

/**
 * @brief 根据Type获取对应的Fence对象
 *
 * 教学要点: 集中管理三个Fence对象的访问
 * - Graphics → m_graphicsFence
 * - Compute → m_computeFence
 * - Copy → m_copyFence
 */
ID3D12Fence* CommandListManager::GetFenceByType(Type type)
{
    switch (type)
    {
    case Type::Graphics:
        return m_graphicsFence.Get();
    case Type::Compute:
        return m_computeFence.Get();
    case Type::Copy:
        return m_copyFence.Get();
    default:
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "GetFenceByType() - Unknown Type");
        return nullptr;
    }
}

/**
 * @brief 根据Type获取对应的Fence值引用
 *
 * 教学要点: 允许修改对应队列的Fence值
 * - 返回引用允许递增操作 (++fenceValue)
 * - 每个队列维护独立的Fence值计数器
 */
uint64_t& CommandListManager::GetFenceValueByType(Type type)
{
    switch (type)
    {
    case Type::Graphics:
        return m_graphicsFenceValue;
    case Type::Compute:
        return m_computeFenceValue;
    case Type::Copy:
        return m_copyFenceValue;
    default:
        // 不应该到达这里，但提供一个静态dummy避免崩溃
        enigma::core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "GetFenceValueByType() - Unknown Type, returning dummy");
        static uint64_t dummy = 0;
        return dummy;
    }
}
