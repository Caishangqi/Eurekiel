#include "Engine/Graphic/Resource/CommandListManager.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Resource/CommandQueueException.hpp"
#include <cassert>
#include <string>

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
using namespace enigma::graphic;

namespace
{
    struct ScopedEventHandle
    {
        HANDLE handle = nullptr;

        ScopedEventHandle()
        {
            handle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        }

        ~ScopedEventHandle()
        {
            if (handle != nullptr)
            {
                CloseHandle(handle);
                handle = nullptr;
            }
        }

        ScopedEventHandle(const ScopedEventHandle&) = delete;
        ScopedEventHandle& operator=(const ScopedEventHandle&) = delete;
    };

    const char* GetWaitResultName(DWORD waitResult)
    {
        switch (waitResult)
        {
        case WAIT_OBJECT_0:
            return "WAIT_OBJECT_0";
        case WAIT_TIMEOUT:
            return "WAIT_TIMEOUT";
        case WAIT_FAILED:
            return "WAIT_FAILED";
        case WAIT_ABANDONED:
            return "WAIT_ABANDONED";
        default:
            return "WAIT_UNKNOWN";
        }
    }

    bool WaitForFenceValue(ID3D12Fence* fence,
                           uint64_t     fenceValue,
                           uint32_t     timeoutMs,
                           const char*  callerContext,
                           const char*  queueName,
                           std::string& errorMessage)
    {
        if (!fence || fenceValue == 0)
        {
            return true;
        }

        if (fence->GetCompletedValue() >= fenceValue)
        {
            return true;
        }

        ScopedEventHandle fenceEvent;
        if (fenceEvent.handle == nullptr)
        {
            errorMessage = Stringf("%s: failed to create wait event for %s queue fence %llu (timeoutMs=%u, lastError=%lu)",
                                   callerContext,
                                   queueName,
                                   fenceValue,
                                   timeoutMs,
                                   static_cast<unsigned long>(GetLastError()));
            return false;
        }

        HRESULT hr = fence->SetEventOnCompletion(fenceValue, fenceEvent.handle);
        if (FAILED(hr))
        {
            errorMessage = Stringf("%s: %s queue wait setup failed at fence %llu (timeoutMs=%u, HRESULT=0x%08X)",
                                   callerContext,
                                   queueName,
                                   fenceValue,
                                   timeoutMs,
                                   hr);
            return false;
        }

        DWORD waitResult = WaitForSingleObject(fenceEvent.handle, timeoutMs);
        if (waitResult != WAIT_OBJECT_0)
        {
            errorMessage = Stringf("%s: %s queue wait failed at fence %llu (timeoutMs=%u, waitResult=%s/%lu, lastError=%lu)",
                                   callerContext,
                                   queueName,
                                   fenceValue,
                                   timeoutMs,
                                   GetWaitResultName(waitResult),
                                   static_cast<unsigned long>(waitResult),
                                   static_cast<unsigned long>(waitResult == WAIT_FAILED ? GetLastError() : 0));
            return false;
        }

        return true;
    }
}

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
        LogError(LogRenderer, "× Failed to initialize CommandListManager reason: device is null");
        ERROR_AND_DIE("Failed to initialize CommandListManager reason: device is null")
    }

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
        LogError(LogRenderer, "Fail to create Compute Queue, Abort Program");
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
        LogError(LogRenderer, "Fail to create Copy Command Queue, Abort Program");
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
     *  Milestone 2.8 架构修复: 每个命令队列使用独立的Fence对象
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
        LogError(LogRenderer, "Fail to create Graphics Fence, Abort Program");
        ERROR_AND_DIE("Fail to create Graphics Fence, Abort Program")
    }
    m_graphicsFence->SetName(L"Enigma Graphics Queue Fence");

    // 创建Compute队列的Fence
    m_computeFenceValue = 0;
    hr                  = device->CreateFence(m_computeFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_computeFence));
    if (FAILED(hr))
    {
        LogError(LogRenderer, "Fail to create Compute Fence, Abort Program");
        ERROR_AND_DIE("Fail to create Compute Fence, Abort Program")
    }
    m_computeFence->SetName(L"Enigma Compute Queue Fence");

    // 创建Copy队列的Fence  核心修复
    m_copyFenceValue = 0;
    hr               = device->CreateFence(m_copyFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_copyFence));
    if (FAILED(hr))
    {
        LogError(LogRenderer, "Fail to create Copy Fence, Abort Program");
        ERROR_AND_DIE("Fail to create Copy Fence, Abort Program")
    }
    m_copyFence->SetName(L"Enigma Copy Queue Fence");

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
    enigma::core::LogInfo(LogRenderer, "Creating command list pools...");
    success &= CreateCommandListPool(Type::Graphics, graphicsCount);
    success &= CreateCommandListPool(Type::Compute, computeCount);
    success &= CreateCommandListPool(Type::Copy, copyCount);

    if (!success)
    {
        enigma::core::LogError(LogRenderer, "× Failed to create command list pools");
        // 清理部分创建的资源
        Shutdown();
        return false;
    }

    enigma::core::LogInfo(LogRenderer,
                          "+ All command list pools created successfully - Graphics:%u, Compute:%u, Copy:%u",
                          graphicsCount, computeCount, copyCount);

    enigma::core::LogInfo(LogRenderer,
                          "+ Graphics command lists created: %u (available: %u)",
                          static_cast<uint32_t>(m_graphicsCommandLists.size()),
                          static_cast<uint32_t>(m_availableGraphicsLists.size()));

    enigma::core::LogInfo(LogRenderer,
                          "+ Compute command lists created: %u (available: %u)",
                          static_cast<uint32_t>(m_computeCommandLists.size()),
                          static_cast<uint32_t>(m_availableComputeLists.size()));

    enigma::core::LogInfo(LogRenderer,
                          "+ Copy command lists created: %u (available: %u)",
                          static_cast<uint32_t>(m_copyCommandLists.size()),
                          static_cast<uint32_t>(m_availableCopyLists.size()));

    m_initialized = true;
    LogInfo(LogRenderer, "+ CommandListManager Initialized");
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

    FlushAllCommandLists();

    // Clear non-owning availability queues first.
    while (!m_availableGraphicsLists.empty()) m_availableGraphicsLists.pop();
    while (!m_availableComputeLists.empty()) m_availableComputeLists.pop();
    while (!m_availableCopyLists.empty()) m_availableCopyLists.pop();

    m_executingLists.clear();

    m_graphicsCommandLists.clear();
    m_computeCommandLists.clear();
    m_copyCommandLists.clear();

    m_graphicsFence.Reset();
    m_graphicsFenceValue = 0;

    m_computeFence.Reset();
    m_computeFenceValue = 0;

    m_copyFence.Reset();
    m_copyFenceValue = 0;

    m_graphicsQueue.Reset();
    m_computeQueue.Reset();
    m_copyQueue.Reset();

    m_initialized = false;
}

bool CommandListManager::WaitForGPU(uint32_t timeoutMs)
{
    try
    {
        std::string errorMessage;
        if (!WaitForFenceValue(m_graphicsFence.Get(), m_graphicsFenceValue, timeoutMs, "CommandListManager::WaitForGPU", "Graphics", errorMessage))
        {
            throw CrossQueueSynchronizationException(errorMessage);
        }

        if (!WaitForFenceValue(m_computeFence.Get(), m_computeFenceValue, timeoutMs, "CommandListManager::WaitForGPU", "Compute", errorMessage))
        {
            throw CrossQueueSynchronizationException(errorMessage);
        }

        if (!WaitForFenceValue(m_copyFence.Get(), m_copyFenceValue, timeoutMs, "CommandListManager::WaitForGPU", "Copy", errorMessage))
        {
            throw CrossQueueSynchronizationException(errorMessage);
        }

        return true;
    }
    catch (const CrossQueueSynchronizationException& exception)
    {
        LogError(LogRenderer, "%s", exception.what());
        return false;
    }
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
    enigma::core::LogInfo(LogRenderer,
                          "Creating %s command list pool with %u command lists...", typeName, count);

    auto* device = D3D12RenderSystem::GetDevice();
    if (!device || count == 0)
    {
        enigma::core::LogError(LogRenderer,
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
            enigma::core::LogError(LogRenderer,
                                   "× Failed to create %s command list %u/%u", typeName, i + 1, count);
            return false;
        }

        // 添加到可用队列
        availableQueue->push(wrapper.get());

        // 移动到容器中
        container->push_back(std::move(wrapper));
    }

    enigma::core::LogInfo(LogRenderer,
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
        enigma::core::LogError(LogRenderer,
                               "CreateCommandList: Device is null");
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
    default:
        enigma::core::LogError(LogRenderer,
                               "CreateCommandList: Invalid type %d", static_cast<int>(type));
        return nullptr;
    }

    // 创建命令分配器 - 管理命令存储的底层内存
    // 教学要点: 每个命令列表需要独立的分配器，避免多线程竞态
    HRESULT hr = device->CreateCommandAllocator(
        d3d12Type,
        IID_PPV_ARGS(&wrapper->commandAllocator)
    );

    if (FAILED(hr))
    {
        enigma::core::LogError(LogRenderer,
                               "CreateCommandList: CreateCommandAllocator failed for type %s, HRESULT=0x%08X",
                               GetTypeName(type), hr);
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
        enigma::core::LogError(LogRenderer,
                               "CreateCommandList: CreateCommandList failed for type %s, HRESULT=0x%08X",
                               GetTypeName(type), hr);
        return nullptr;
    }

    // DirectX 12创建的命令列表默认是Recording状态
    // 我们需要关闭它，使其处于可提交状态
    hr = wrapper->commandList->Close();
    if (FAILED(hr))
    {
        enigma::core::LogError(LogRenderer,
                               "CreateCommandList: Close() failed for type %s, HRESULT=0x%08X",
                               GetTypeName(type), hr);
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

    enigma::core::LogInfo(LogRenderer,
                          "CreateCommandList: Successfully created %s command list",
                          GetTypeName(type));

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

    auto& availableQueue = GetAvailableQueue(type);
    if (availableQueue.empty())
    {
        enigma::core::LogError(LogRenderer,
                               "- AcquireCommandList failed - no available %s command lists in queue", typeName);
        enigma::core::LogError(LogRenderer,
                               "   This indicates the command list pool was not properly created");
        return nullptr;
    }

    CommandListWrapper* wrapper = availableQueue.front();
    availableQueue.pop();

    assert(wrapper != nullptr);
    assert(wrapper->state == State::Closed);
    
    HRESULT hr = wrapper->commandAllocator->Reset();
    if (FAILED(hr))
    {
        availableQueue.push(wrapper);
        return nullptr;
    }
    
    hr = wrapper->commandList->Reset(wrapper->commandAllocator.Get(), nullptr);
    if (FAILED(hr))
    {
        availableQueue.push(wrapper);
        return nullptr;
    }

    wrapper->state      = State::Recording;
    wrapper->fenceValue = 0;

    if (!debugName.empty())
    {
        wrapper->debugName = debugName;
        std::wstring wDebugName(debugName.begin(), debugName.end());
        wrapper->commandList->SetName(wDebugName.c_str());
    }

    return wrapper->commandList.Get();
}

/**
 * Acquire a command list using an external command allocator (e.g. from FrameContext).
 * The wrapper's internal allocator is left untouched - only the command list is
 * Reset with the provided allocator. If no wrappers are available, tries to
 * recycle completed ones before failing.
 */
ID3D12GraphicsCommandList* CommandListManager::AcquireCommandList(
    Type type, ID3D12CommandAllocator* externalAllocator, const std::string& debugName)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!externalAllocator)
    {
        enigma::core::LogError(LogRenderer,
                               "AcquireCommandList: externalAllocator is null");
        return nullptr;
    }

    auto& availableQueue = GetAvailableQueue(type);

    // If no wrappers available, try recycling completed ones first
    if (availableQueue.empty())
    {
        UpdateCompletedCommandLists();
    }

    if (availableQueue.empty())
    {
        enigma::core::LogError(LogRenderer,
                               "AcquireCommandList: no available %s command lists after recycle attempt",
                               GetTypeName(type));
        return nullptr;
    }

    CommandListWrapper* wrapper = availableQueue.front();
    availableQueue.pop();

    assert(wrapper != nullptr);
    assert(wrapper->state == State::Closed);

    // Reset command list with external allocator (skip internal allocator reset)
    HRESULT hr = wrapper->commandList->Reset(externalAllocator, nullptr);
    if (FAILED(hr))
    {
        enigma::core::LogError(LogRenderer,
                               "AcquireCommandList: Reset with external allocator failed, HRESULT=0x%08X", hr);
        availableQueue.push(wrapper);
        return nullptr;
    }

    wrapper->state      = State::Recording;
    wrapper->fenceValue = 0;

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
QueueSubmissionToken CommandListManager::SubmitCommandList(ID3D12GraphicsCommandList* commandList)
{
    if (!commandList || !m_initialized)
    {
        return {};
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 查找对应的包装器
    CommandListWrapper* wrapper = FindWrapper(commandList);
    if (!wrapper || wrapper->state != State::Recording)
    {
        enigma::core::LogError(LogRenderer,
                               "SubmitCommandList() - Invalid wrapper or state for command list");
        return {};
    }

    // 关闭命令列表 - 结束录制阶段
    HRESULT hr = commandList->Close();
    if (FAILED(hr))
    {
        enigma::core::LogError(LogRenderer,
                               "SubmitCommandList() - Failed to close command list, HRESULT=0x%08X", hr);
        return {};
    }

    wrapper->state = State::Closed;

    // 获取对应的命令队列
    ID3D12CommandQueue* queue = GetCommandQueue(wrapper->type);
    if (!queue)
    {
        enigma::core::LogError(LogRenderer,
                               "SubmitCommandList() - Failed to get queue for type %s",
                               GetTypeName(wrapper->type));
        return {};
    }

    // 提交命令列表到GPU执行
    ID3D12CommandList* commandLists[] = {commandList};
    queue->ExecuteCommandLists(1, commandLists);

    //  Milestone 2.8: 根据Type选择对应的Fence对象和Fence值
    // 教学要点: 每个队列使用独立的Fence避免竞态条件
    uint64_t&    fenceValue = GetFenceValueByType(wrapper->type);
    ID3D12Fence* fence      = GetFenceByType(wrapper->type);

    if (!fence)
    {
        enigma::core::LogError(LogRenderer,
                               "SubmitCommandList() - Failed to get Fence for type %s",
                               GetTypeName(wrapper->type));
        return {};
    }

    // 增加围栏值并发送信号 - 标记这批命令的完成时机
    ++fenceValue;
    hr = queue->Signal(fence, fenceValue);
    if (FAILED(hr))
    {
        enigma::core::LogError(LogRenderer,
                               "SubmitCommandList() - Signal failed for %s queue",
                               GetTypeName(wrapper->type));
        return {};
    }

    // 更新包装器状态
    wrapper->state      = State::Executing;
    wrapper->fenceValue = fenceValue;

    // 添加到执行中列表
    m_executingLists.push_back(wrapper);

    return { wrapper->type, fenceValue };
}

/**
 * @brief 批量提交多个命令列表
 *
 * 教学要点: 批量提交可以减少API调用开销
 *
 *  Milestone 2.8: 重构为支持三个独立Fence
 * - 验证所有命令列表为同一类型
 * - 使用对应类型的Fence和队列
 */
QueueSubmissionToken CommandListManager::SubmitCommandLists(ID3D12GraphicsCommandList* const* commandLists, uint32_t count)
{
    if (!commandLists || count == 0 || !m_initialized)
    {
        return {};
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
            enigma::core::LogError(LogRenderer,
                                   "SubmitCommandLists() - Invalid wrapper or state for command list %u", i);
            return {};
        }

        //  Milestone 2.8: 验证所有命令列表类型一致
        if (firstWrapper)
        {
            batchType    = wrapper->type;
            firstWrapper = false;
        }
        else if (wrapper->type != batchType)
        {
            enigma::core::LogError(LogRenderer,
                                   "SubmitCommandLists() - Mixed types detected! Expected %s, got %s at index %u",
                                   GetTypeName(batchType), GetTypeName(wrapper->type), i);
            return {};
        }

        HRESULT hr = commandLists[i]->Close();
        if (FAILED(hr))
        {
            enigma::core::LogError(LogRenderer,
                                   "SubmitCommandLists() - Failed to close command list %u", i);
            return {};
        }

        wrapper->state = State::Closed;
        wrappers.push_back(wrapper);
    }

    //  Milestone 2.8: 获取对应类型的命令队列
    ID3D12CommandQueue* queue = GetCommandQueue(batchType);
    if (!queue)
    {
        enigma::core::LogError(LogRenderer,
                               "SubmitCommandLists() - Failed to get queue for type %s",
                               GetTypeName(batchType));
        return {};
    }

    // 批量提交
    queue->ExecuteCommandLists(count, reinterpret_cast<ID3D12CommandList* const*>(commandLists));

    //  Milestone 2.8: 根据Type选择对应的Fence对象和Fence值
    uint64_t&    fenceValue = GetFenceValueByType(batchType);
    ID3D12Fence* fence      = GetFenceByType(batchType);

    if (!fence)
    {
        enigma::core::LogError(LogRenderer,
                               "SubmitCommandLists() - Failed to get Fence for type %s",
                               GetTypeName(batchType));
        return {};
    }

    // Advance the queue fence and signal the submitted batch.
    ++fenceValue;
    HRESULT hr = queue->Signal(fence, fenceValue);
    if (FAILED(hr))
    {
        enigma::core::LogError(LogRenderer,
                               "SubmitCommandLists() - Signal failed for %s queue",
                               GetTypeName(batchType));
        return {};
    }

    // Mark every submitted wrapper as executing on this fence value.
    for (CommandListWrapper* wrapper : wrappers)
    {
        wrapper->state      = State::Executing;
        wrapper->fenceValue = fenceValue;
        m_executingLists.push_back(wrapper);
    }

    return { batchType, fenceValue };
}

// ========================================================================
// Synchronization interface implementation
// ========================================================================

/**
 * @brief Wait until the queue-scoped submission token completes.
 *
 * Uses the token queue identity to select the correct fence for CPU-side waiting.
 */
bool CommandListManager::WaitForSubmission(const QueueSubmissionToken& token, uint32_t timeoutMs)
{
    try
    {
        if (!token.IsValid())
        {
            throw InvalidQueueSubmissionTokenException(
                Stringf("CommandListManager::WaitForSubmission: invalid submission token for queue %s (fence=%llu, timeoutMs=%u)",
                        GetTypeName(token.queueType),
                        token.fenceValue,
                        timeoutMs));
        }

        ID3D12Fence* fence = GetFenceByType(token.queueType);
        if (!fence)
        {
            throw CrossQueueSynchronizationException(
                Stringf("CommandListManager::WaitForSubmission: fence is unavailable for queue %s (fence=%llu, timeoutMs=%u)",
                        GetTypeName(token.queueType),
                        token.fenceValue,
                        timeoutMs));
        }

        std::string errorMessage;
        if (!WaitForFenceValue(fence,
                               token.fenceValue,
                               timeoutMs,
                               "CommandListManager::WaitForSubmission",
                               GetTypeName(token.queueType),
                               errorMessage))
        {
            throw CrossQueueSynchronizationException(errorMessage);
        }

        return true;
    }
    catch (const InvalidQueueSubmissionTokenException& exception)
    {
        LogWarn(LogRenderer, "%s", exception.what());
        return false;
    }
    catch (const CrossQueueSynchronizationException& exception)
    {
        LogError(LogRenderer, "%s", exception.what());
        return false;
    }
}

// WaitForGPU is defined earlier in this file.

/**
 * @brief Query whether a queue-scoped submission token has completed.
 *
 * Uses the token queue identity to query the correct fence.
 */
bool CommandListManager::IsSubmissionCompleted(const QueueSubmissionToken& token) const
{
    if (!token.IsValid())
    {
        return false;
    }

    ID3D12Fence* fence = const_cast<CommandListManager*>(this)->GetFenceByType(token.queueType);
    if (!fence)
    {
        return false;
    }

    return fence->GetCompletedValue() >= token.fenceValue;
}

/**
 * @brief Get a queue completion snapshot for all command queues.
 */
QueueFenceSnapshot CommandListManager::GetCompletedFenceSnapshot() const
{
    QueueFenceSnapshot snapshot = {};
    snapshot.graphicsCompleted  = m_graphicsFence ? m_graphicsFence->GetCompletedValue() : 0;
    snapshot.computeCompleted   = m_computeFence ? m_computeFence->GetCompletedValue() : 0;
    snapshot.copyCompleted      = m_copyFence ? m_copyFence->GetCompletedValue() : 0;
    return snapshot;
}

QueueSubmittedFenceSnapshot CommandListManager::GetLastSubmittedFenceSnapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    QueueSubmittedFenceSnapshot snapshot = {};
    snapshot.graphicsSubmitted           = m_graphicsFenceValue;
    snapshot.computeSubmitted            = m_computeFenceValue;
    snapshot.copySubmitted               = m_copyFenceValue;
    return snapshot;
}

bool CommandListManager::InsertQueueWait(Type waitingQueue, const QueueSubmissionToken& producerToken)
{
    try
    {
        if (!m_initialized)
        {
            throw CrossQueueSynchronizationException("CommandListManager::InsertQueueWait: manager is not initialized");
        }

        if (!producerToken.IsValid())
        {
            throw InvalidQueueSubmissionTokenException(
                Stringf("CommandListManager::InsertQueueWait: invalid producer token for queue %s",
                        GetTypeName(producerToken.queueType)));
        }

        if (waitingQueue == producerToken.queueType)
        {
            return true;
        }

        ID3D12CommandQueue* waitingCommandQueue = GetCommandQueue(waitingQueue);
        ID3D12Fence*        producerFence       = GetFenceByType(producerToken.queueType);

        if (!waitingCommandQueue || !producerFence)
        {
            throw CrossQueueSynchronizationException(
                Stringf("CommandListManager::InsertQueueWait: failed to resolve wait from %s to %s",
                        GetTypeName(producerToken.queueType),
                        GetTypeName(waitingQueue)));
        }

        HRESULT hr = waitingCommandQueue->Wait(producerFence, producerToken.fenceValue);
        if (FAILED(hr))
        {
            throw CrossQueueSynchronizationException(
                Stringf("CommandListManager::InsertQueueWait: wait from %s to %s failed at fence %llu (HRESULT=0x%08X)",
                        GetTypeName(producerToken.queueType),
                        GetTypeName(waitingQueue),
                        producerToken.fenceValue,
                        hr));
        }

        return true;
    }
    catch (const InvalidQueueSubmissionTokenException& exception)
    {
        LogWarn(LogRenderer, "%s", exception.what());
        return false;
    }
    catch (const CrossQueueSynchronizationException& exception)
    {
        LogWarn(LogRenderer, "%s", exception.what());
        return false;
    }
}

uint64_t CommandListManager::ExecuteCommandList(ID3D12GraphicsCommandList* commandList)
{
    return SubmitCommandList(commandList).fenceValue;
}

uint64_t CommandListManager::ExecuteCommandLists(ID3D12GraphicsCommandList* const* commandLists, uint32_t count)
{
    return SubmitCommandLists(commandLists, count).fenceValue;
}

bool CommandListManager::WaitForFence(uint64_t fenceValue, uint32_t timeoutMs)
{
    if (fenceValue == 0)
    {
        return false;
    }

    CommandListWrapper* wrapper = FindWrapperByFenceValue(fenceValue);
    if (wrapper)
    {
        return WaitForSubmission({ wrapper->type, fenceValue }, timeoutMs);
    }

    QueueFenceSnapshot snapshot = GetCompletedFenceSnapshot();
    if (snapshot.graphicsCompleted >= fenceValue ||
        snapshot.computeCompleted >= fenceValue ||
        snapshot.copyCompleted >= fenceValue)
    {
        return true;
    }

    enigma::core::LogWarn(LogRenderer,
                          "WaitForFence() - Falling back to Graphics queue for untracked fence value %llu",
                          fenceValue);
    return WaitForSubmission({ Type::Graphics, fenceValue }, timeoutMs);
}

bool CommandListManager::IsFenceCompleted(uint64_t fenceValue) const
{
    if (fenceValue == 0)
    {
        return false;
    }

    CommandListWrapper* wrapper = const_cast<CommandListManager*>(this)->FindWrapperByFenceValue(fenceValue);
    if (wrapper)
    {
        return IsSubmissionCompleted({ wrapper->type, fenceValue });
    }

    QueueFenceSnapshot snapshot = GetCompletedFenceSnapshot();
    return snapshot.graphicsCompleted >= fenceValue ||
           snapshot.computeCompleted >= fenceValue ||
           snapshot.copyCompleted >= fenceValue;
}

uint64_t CommandListManager::GetCompletedFenceValue() const
{
    QueueFenceSnapshot snapshot = GetCompletedFenceSnapshot();
    uint64_t           minCompletedValue = UINT64_MAX;

    minCompletedValue = (std::min)(minCompletedValue, snapshot.graphicsCompleted);
    minCompletedValue = (std::min)(minCompletedValue, snapshot.computeCompleted);
    minCompletedValue = (std::min)(minCompletedValue, snapshot.copyCompleted);

    if (minCompletedValue == UINT64_MAX)
    {
        return 0;
    }

    return minCompletedValue;
}

void CommandListManager::UpdateCompletedCommandLists()
{
    if (m_executingLists.empty())
    {
        return;
    }
    size_t recycledCount = 0;

    auto it = m_executingLists.begin();
    while (it != m_executingLists.end())
    {
        CommandListWrapper* wrapper = *it;

        ID3D12Fence* fence = GetFenceByType(wrapper->type);
        if (!fence)
        {
            enigma::core::LogError(LogRenderer,
                                   "UpdateCompletedCommandLists() - Fence is null for type %s",
                                   GetTypeName(wrapper->type));
            ++it;
            continue;
        }

        uint64_t completedValue = fence->GetCompletedValue();

        enigma::core::LogDebug(LogRenderer,
                               "  Checking wrapper[%s]: fenceValue=%llu (completed=%llu, canRecycle=%s)",
                               GetTypeName(wrapper->type),
                               wrapper->fenceValue,
                               completedValue,
                               (wrapper->fenceValue <= completedValue) ? "YES" : "NO");

        if (wrapper->fenceValue <= completedValue)
        {
            wrapper->state = State::Closed;

            auto& availableQueue = GetAvailableQueue(wrapper->type);
            availableQueue.push(wrapper);

            /*enigma::core::LogInfo(LogRenderer,
                                  "  + Recycled %s command list (fenceValue=%llu)",
                                  GetTypeName(wrapper->type), wrapper->fenceValue);*/
            it = m_executingLists.erase(it);
            recycledCount++;
        }
        else
        {
            enigma::core::LogWarn(LogRenderer, "  ! Cannot recycle %s command list - fenceValue(%llu) > completedValue(%llu)", GetTypeName(wrapper->type), wrapper->fenceValue, completedValue);
            ++it;
        }
    }
}

void CommandListManager::FlushAllCommandLists()
{
    if (!m_initialized)
    {
        return;
    }

    if (!WaitForGPU())
    {
        ERROR_AND_DIE("CommandListManager::FlushAllCommandLists: failed to synchronize all active queues");
    }

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
 * @brief 根据 queue-scoped token 查找对应的 CommandListWrapper
 */
CommandListManager::CommandListWrapper* CommandListManager::FindWrapperBySubmissionToken(const QueueSubmissionToken& token)
{
    if (!token.IsValid())
    {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto* wrapper : m_executingLists)
    {
        if (wrapper &&
            wrapper->type == token.queueType &&
            wrapper->fenceValue == token.fenceValue)
        {
            return wrapper;
        }
    }

    return nullptr;
}

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

    CommandListWrapper* matchedWrapper = nullptr;
    for (auto* wrapper : m_executingLists)
    {
        if (wrapper && wrapper->fenceValue == fenceValue)
        {
            if (matchedWrapper != nullptr && matchedWrapper->type != wrapper->type)
            {
                enigma::core::LogWarn(LogRenderer,
                                      "FindWrapperByFenceValue() - Ambiguous raw fence lookup for fence %llu across %s and %s queues",
                                      fenceValue,
                                      GetTypeName(matchedWrapper->type),
                                      GetTypeName(wrapper->type));
                return nullptr;
            }

            matchedWrapper = wrapper;
        }
    }

    return matchedWrapper;
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
        enigma::core::LogError(LogRenderer,
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
        enigma::core::LogError(LogRenderer,
                               "GetFenceValueByType() - Unknown Type, returning dummy");
        static uint64_t dummy = 0;
        return dummy;
    }
}
