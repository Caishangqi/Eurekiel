#include "GlobalDescriptorHeapManager.hpp"
#include "../Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include <cassert>
#include <algorithm>

using namespace enigma::graphic;

// ==================== DescriptorAllocation 实现 ====================

/**
 * @brief DescriptorAllocation默认构造函数
 *
 * 教学要点: 初始化所有成员为无效状态，确保对象处于明确的初始状态
 */
GlobalDescriptorHeapManager::DescriptorAllocation::DescriptorAllocation()
    : cpuHandle{} // 零初始化CPU句柄
      , gpuHandle{} // 零初始化GPU句柄
      , heapIndex(UINT32_MAX) // 无效索引
      , heapType(HeapType::CBV_SRV_UAV) // 默认堆类型
      , isValid(false) // 标记为无效
{
    // 教学注释: 使用UINT32_MAX作为无效索引是常见做法
    // 这样可以很容易地检测未初始化或无效的分配
}

/**
 * @brief 重置分配状态为无效
 */
void GlobalDescriptorHeapManager::DescriptorAllocation::Reset()
{
    cpuHandle = {};
    gpuHandle = {};
    heapIndex = UINT32_MAX;
    heapType  = HeapType::CBV_SRV_UAV;
    isValid   = false;
}

// ==================== DescriptorHeap 实现 ====================

/**
 * @brief DescriptorHeap默认构造函数
 */
GlobalDescriptorHeapManager::DescriptorHeap::DescriptorHeap()
    : heap(nullptr)
      , type(HeapType::CBV_SRV_UAV)
      , capacity(0)
      , used(0)
      , descriptorSize(0)
      , cpuStart{}
      , gpuStart{}
{
    // TODO: 稍后完成完整实现
}

/**
 * @brief 获取指定索引的CPU句柄
 */
D3D12_CPU_DESCRIPTOR_HANDLE GlobalDescriptorHeapManager::DescriptorHeap::GetCPUHandle(uint32_t index) const
{
    // TODO: 稍后完成完整实现
    D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuStart;
    handle.ptr += index * descriptorSize;
    return handle;
}

/**
 * @brief 获取指定索引的GPU句柄
 */
D3D12_GPU_DESCRIPTOR_HANDLE GlobalDescriptorHeapManager::DescriptorHeap::GetGPUHandle(uint32_t index) const
{
    // TODO: 稍后完成完整实现
    D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuStart;
    handle.ptr += index * descriptorSize;
    return handle;
}

// ==================== GlobalDescriptorHeapManager 实现 ====================

/**
 * @brief DescriptorHeapManager构造函数
 */
GlobalDescriptorHeapManager::GlobalDescriptorHeapManager()
    : m_cbvSrvUavHeap(nullptr)
      , m_rtvHeap(nullptr)
      , m_dsvHeap(nullptr)
      , m_samplerHeap(nullptr)
      , m_cbvSrvUavCapacity(0)
      , m_rtvCapacity(0)
      , m_dsvCapacity(0)
      , m_samplerCapacity(0)
      , m_nextFreeCbvSrvUav(0)
      , m_nextFreeRtv(0)
      , m_nextFreeDsv(0)
      , m_nextFreeSampler(0)
      , m_totalCbvSrvUavAllocated(0)
      , m_totalRtvAllocated(0)
      , m_totalDsvAllocated(0)
      , m_totalSamplerAllocated(0)
      , m_peakCbvSrvUavUsed(0)
      , m_peakRtvUsed(0)
      , m_peakDsvUsed(0)
      , m_peakSamplerUsed(0)
      , m_initialized(false)
{
    // TODO: 稍后完成完整实现
}

/**
 * @brief DescriptorHeapManager析构函数
 */
GlobalDescriptorHeapManager::~GlobalDescriptorHeapManager()
{
    // TODO: 稍后完成完整实现 - 调用Shutdown清理资源
    Shutdown();
}

/**
 * @brief 初始化描述符堆管理器
 */
bool GlobalDescriptorHeapManager::Initialize(uint32_t cbvSrvUavCapacity, uint32_t rtvCapacity, uint32_t dsvCapacity, uint32_t samplerCapacity)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 防止重复初始化
    if (m_initialized)
    {
        return false;
    }

    // 1. 验证D3D12RenderSystem已初始化
    if (!D3D12RenderSystem::IsInitialized())
    {
        return false;
    }

    // 2. 获取D3D12设备
    auto device = D3D12RenderSystem::GetDevice();
    if (!device)
    {
        return false;
    }

    try
    {
        // 3. 创建4种描述符堆
        m_cbvSrvUavHeap = CreateDescriptorHeap(HeapType::CBV_SRV_UAV, cbvSrvUavCapacity);
        m_rtvHeap       = CreateDescriptorHeap(HeapType::RTV, rtvCapacity);
        m_dsvHeap       = CreateDescriptorHeap(HeapType::DSV, dsvCapacity);
        m_samplerHeap   = CreateDescriptorHeap(HeapType::Sampler, samplerCapacity);

        // 4. 验证所有堆创建成功
        if (!m_cbvSrvUavHeap || !m_rtvHeap || !m_dsvHeap || !m_samplerHeap)
        {
            Shutdown();
            return false;
        }

        // 5. 初始化位图数组
        m_cbvSrvUavUsed.resize(cbvSrvUavCapacity, false);
        m_rtvUsed.resize(rtvCapacity, false);
        m_dsvUsed.resize(dsvCapacity, false);
        m_samplerUsed.resize(samplerCapacity, false);

        // 6. 设置容量参数
        m_cbvSrvUavCapacity = cbvSrvUavCapacity;
        m_rtvCapacity       = rtvCapacity;
        m_dsvCapacity       = dsvCapacity;
        m_samplerCapacity   = samplerCapacity;

        // 7. 重置统计和索引
        m_nextFreeCbvSrvUav = 0;
        m_nextFreeRtv       = 0;
        m_nextFreeDsv       = 0;
        m_nextFreeSampler   = 0;

        m_totalCbvSrvUavAllocated = 0;
        m_totalRtvAllocated       = 0;
        m_totalDsvAllocated       = 0;
        m_totalSamplerAllocated   = 0;

        m_peakCbvSrvUavUsed = 0;
        m_peakRtvUsed       = 0;
        m_peakDsvUsed       = 0;
        m_peakSamplerUsed   = 0;

        m_initialized = true;
        return true;
    }
    catch (...)
    {
        Shutdown();
        return false;
    }
}

/**
 * @brief 释放所有资源
 */
void GlobalDescriptorHeapManager::Shutdown()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 释放所有描述符堆
    m_cbvSrvUavHeap.reset();
    m_rtvHeap.reset();
    m_dsvHeap.reset();
    m_samplerHeap.reset();

    // 清理所有使用状态数组
    m_cbvSrvUavUsed.clear();
    m_rtvUsed.clear();
    m_dsvUsed.clear();
    m_samplerUsed.clear();

    // 重置所有状态
    m_cbvSrvUavCapacity = 0;
    m_rtvCapacity       = 0;
    m_dsvCapacity       = 0;
    m_samplerCapacity   = 0;

    m_nextFreeCbvSrvUav = 0;
    m_nextFreeRtv       = 0;
    m_nextFreeDsv       = 0;
    m_nextFreeSampler   = 0;

    m_totalCbvSrvUavAllocated = 0;
    m_totalRtvAllocated       = 0;
    m_totalDsvAllocated       = 0;
    m_totalSamplerAllocated   = 0;

    m_peakCbvSrvUavUsed = 0;
    m_peakRtvUsed       = 0;
    m_peakDsvUsed       = 0;
    m_peakSamplerUsed   = 0;

    m_initialized = false;
}

/**
 * @brief 检查是否已初始化
 */
bool GlobalDescriptorHeapManager::IsInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

/**
 * @brief 分配CBV/SRV/UAV描述符
 */
GlobalDescriptorHeapManager::DescriptorAllocation GlobalDescriptorHeapManager::AllocateCbvSrvUav()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    DescriptorAllocation allocation;

    // 1. 检查初始化状态
    if (!m_initialized || !m_cbvSrvUavHeap)
    {
        return allocation; // 返回无效分配
    }

    // 2. 分配索引
    uint32_t index = AllocateIndex(HeapType::CBV_SRV_UAV);
    if (index == UINT32_MAX)
    {
        return allocation; // 分配失败
    }

    // 3. 填充分配信息
    allocation.heapIndex = index;
    allocation.heapType  = HeapType::CBV_SRV_UAV;
    allocation.cpuHandle = m_cbvSrvUavHeap->GetCPUHandle(index);
    allocation.gpuHandle = m_cbvSrvUavHeap->GetGPUHandle(index);
    allocation.isValid   = true;

    return allocation;
}

/**
 * @brief 分配Sampler描述符
 */
GlobalDescriptorHeapManager::DescriptorAllocation GlobalDescriptorHeapManager::AllocateSampler()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    DescriptorAllocation allocation;

    // 1. 检查初始化状态
    if (!m_initialized || !m_samplerHeap)
    {
        return allocation; // 返回无效分配
    }

    // 2. 分配索引
    uint32_t index = AllocateIndex(HeapType::Sampler);
    if (index == UINT32_MAX)
    {
        return allocation; // 分配失败
    }

    // 3. 填充分配信息
    allocation.heapIndex = index;
    allocation.heapType  = HeapType::Sampler;
    allocation.cpuHandle = m_samplerHeap->GetCPUHandle(index);
    allocation.gpuHandle = m_samplerHeap->GetGPUHandle(index);
    allocation.isValid   = true;

    return allocation;
}

/**
 * @brief 分配RTV描述符 (渲染目标视图)
 */
GlobalDescriptorHeapManager::DescriptorAllocation GlobalDescriptorHeapManager::AllocateRtv()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    DescriptorAllocation allocation;

    // 1. 检查初始化状态
    if (!m_initialized || !m_rtvHeap)
    {
        return allocation; // 返回无效分配
    }

    // 2. 分配索引
    uint32_t index = AllocateIndex(HeapType::RTV);
    if (index == UINT32_MAX)
    {
        return allocation; // 分配失败
    }

    // 3. 填充分配信息
    allocation.heapIndex = index;
    allocation.heapType  = HeapType::RTV;
    allocation.cpuHandle = m_rtvHeap->GetCPUHandle(index);
    // 注意：RTV堆不是SHADER_VISIBLE，所以GPU句柄无效
    allocation.gpuHandle = {};
    allocation.isValid   = true;

    return allocation;
}

/**
 * @brief 分配DSV描述符 (深度模板视图)
 */
GlobalDescriptorHeapManager::DescriptorAllocation GlobalDescriptorHeapManager::AllocateDsv()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    DescriptorAllocation allocation;

    // 1. 检查初始化状态
    if (!m_initialized || !m_dsvHeap)
    {
        return allocation; // 返回无效分配
    }

    // 2. 分配索引
    uint32_t index = AllocateIndex(HeapType::DSV);
    if (index == UINT32_MAX)
    {
        return allocation; // 分配失败
    }

    // 3. 填充分配信息
    allocation.heapIndex = index;
    allocation.heapType  = HeapType::DSV;
    allocation.cpuHandle = m_dsvHeap->GetCPUHandle(index);
    // 注意：DSV堆不是SHADER_VISIBLE，所以GPU句柄无效
    allocation.gpuHandle = {};
    allocation.isValid   = true;

    return allocation;
}

/**
 * @brief 批量分配CBV/SRV/UAV描述符
 */
std::vector<GlobalDescriptorHeapManager::DescriptorAllocation> GlobalDescriptorHeapManager::BatchAllocateCbvSrvUav(uint32_t count)
{
    // TODO: 稍后完成完整实现
    std::vector<DescriptorAllocation> allocations;
    allocations.reserve(count);

    for (uint32_t i = 0; i < count; ++i)
    {
        allocations.push_back(AllocateCbvSrvUav());
    }

    return allocations;
}

/**
 * @brief 释放CBV/SRV/UAV描述符
 *
 * 教学要点:
 * 1. DirectX 12描述符堆中的资源释放机制
 * 2. Bindless资源管理的核心 - 索引回收和复用
 * 3. 线程安全的资源管理实践
 * 4. 验证机制确保资源释放的正确性
 *
 * API来源: 基于DirectX 12最佳实践和现代C++资源管理模式
 * - std::lock_guard: C++11 RAII锁管理
 * - DescriptorAllocation: 自定义描述符分配结构
 * - FreeIndex: 内部索引回收方法
 *
 * @param allocation 要释放的描述符分配信息
 * @return 成功返回true，失败返回false
 */
bool GlobalDescriptorHeapManager::FreeCbvSrvUav(const DescriptorAllocation& allocation)
{
    // 教学注释: 获取互斥锁确保线程安全
    // 这是现代C++多线程编程的标准做法，使用RAII管理锁生命周期
    std::lock_guard<std::mutex> lock(m_mutex);

    // 教学注释: 验证分配有效性 - 防御性编程的重要实践
    // 1. 检查分配是否有效
    // 2. 检查堆类型是否匹配CBV_SRV_UAV
    // 这种验证机制是DirectX 12编程中避免GPU崩溃的关键
    if (!allocation.isValid || allocation.heapType != HeapType::CBV_SRV_UAV)
    {
        // 教学注释: 返回false表示释放失败，调用者可以根据返回值判断操作结果
        return false;
    }

    // 教学注释: 验证描述符堆管理器的初始化状态
    // 在DirectX 12中，未初始化的堆操作会导致运行时错误
    if (!m_initialized || !m_cbvSrvUavHeap)
    {
        return false;
    }

    // 教学注释: 调用内部索引释放方法
    // FreeIndex方法实现了以下核心功能:
    // 1. 将使用状态位图中对应位置设为false
    // 2. 优化下次分配的起始搜索位置
    // 3. 更新使用统计信息
    // 这是Bindless资源系统中索引回收的核心机制
    FreeIndex(HeapType::CBV_SRV_UAV, allocation.heapIndex);

    // 教学注释: 返回true表示释放成功
    // 此时:
    // 1. 描述符索引已标记为可用
    // 2. 统计信息已更新
    // 3. 该索引可被后续AllocateCbvSrvUav()重新分配
    // 4. 对应的GPU资源仍然存在，只是描述符索引被回收
    return true;
}

/**
 * @brief 释放Sampler描述符
 */
bool GlobalDescriptorHeapManager::FreeSampler(const DescriptorAllocation& allocation)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. 验证分配有效性
    if (!allocation.isValid || allocation.heapType != HeapType::Sampler)
    {
        return false;
    }

    // 2. 验证初始化状态
    if (!m_initialized || !m_samplerHeap)
    {
        return false;
    }

    // 3. 释放索引
    FreeIndex(HeapType::Sampler, allocation.heapIndex);
    return true;
}

/**
 * @brief 释放RTV描述符
 */
bool GlobalDescriptorHeapManager::FreeRtv(const DescriptorAllocation& allocation)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. 验证分配有效性
    if (!allocation.isValid || allocation.heapType != HeapType::RTV)
    {
        return false;
    }

    // 2. 验证初始化状态
    if (!m_initialized || !m_rtvHeap)
    {
        return false;
    }

    // 3. 释放索引
    FreeIndex(HeapType::RTV, allocation.heapIndex);
    return true;
}

/**
 * @brief 释放DSV描述符
 */
bool GlobalDescriptorHeapManager::FreeDsv(const DescriptorAllocation& allocation)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. 验证分配有效性
    if (!allocation.isValid || allocation.heapType != HeapType::DSV)
    {
        return false;
    }

    // 2. 验证初始化状态
    if (!m_initialized || !m_dsvHeap)
    {
        return false;
    }

    // 3. 释放索引
    FreeIndex(HeapType::DSV, allocation.heapIndex);
    return true;
}

/**
 * @brief 获取CBV/SRV/UAV描述符堆
 */
ID3D12DescriptorHeap* GlobalDescriptorHeapManager::GetCbvSrvUavHeap() const
{
    // TODO: 稍后完成完整实现
    return m_cbvSrvUavHeap ? m_cbvSrvUavHeap->heap.Get() : nullptr;
}

/**
 * @brief 获取Sampler描述符堆
 */
ID3D12DescriptorHeap* GlobalDescriptorHeapManager::GetSamplerHeap() const
{
    return m_samplerHeap ? m_samplerHeap->heap.Get() : nullptr;
}

/**
 * @brief 获取RTV描述符堆 (渲染目标视图堆)
 */
ID3D12DescriptorHeap* GlobalDescriptorHeapManager::GetRtvHeap() const
{
    return m_rtvHeap ? m_rtvHeap->heap.Get() : nullptr;
}

/**
 * @brief 获取DSV描述符堆 (深度模板视图堆)
 */
ID3D12DescriptorHeap* GlobalDescriptorHeapManager::GetDsvHeap() const
{
    return m_dsvHeap ? m_dsvHeap->heap.Get() : nullptr;
}

/**
 * @brief 设置描述符堆到命令列表
 */
void GlobalDescriptorHeapManager::SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList) const
{
    // TODO: 稍后完成完整实现
    if (!commandList)
        return;

    std::vector<ID3D12DescriptorHeap*> heaps;

    if (m_cbvSrvUavHeap && m_cbvSrvUavHeap->heap)
        heaps.push_back(m_cbvSrvUavHeap->heap.Get());

    if (m_samplerHeap && m_samplerHeap->heap)
        heaps.push_back(m_samplerHeap->heap.Get());

    if (!heaps.empty())
    {
        commandList->SetDescriptorHeaps(static_cast<UINT>(heaps.size()), heaps.data());
    }
}

// ==================== SM6.6 Bindless索引创建接口实现 ====================

/**
 * @brief 在指定索引创建Shader Resource View
 *
 * 教学要点:
 * 1. SM6.6架构核心：索引由BindlessIndexAllocator分配，描述符在此创建
 * 2. 全局堆支持DIRECTLY_INDEXED，HLSL可用索引直接访问
 * 3. 职责分离：BindlessIndexAllocator管理索引，GlobalDescriptorHeapManager管理堆
 */
void GlobalDescriptorHeapManager::CreateShaderResourceView(
    ID3D12Device* device,
    ID3D12Resource* resource,
    const D3D12_SHADER_RESOURCE_VIEW_DESC* desc,
    uint32_t index)
{
    if (!m_initialized || !m_cbvSrvUavHeap)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateShaderResourceView: not initialized");
        return;
    }

    // 验证索引范围
    if (index >= m_cbvSrvUavCapacity)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateShaderResourceView: index %u out of range (capacity: %u)",
                      index, m_cbvSrvUavCapacity);
        return;
    }

    // 获取指定索引的CPU句柄
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_cbvSrvUavHeap->GetCPUHandle(index);

    // 在全局堆的指定索引位置创建SRV
    device->CreateShaderResourceView(resource, desc, cpuHandle);

    core::LogInfo("GlobalDescriptorHeapManager",
                 "CreateShaderResourceView: created at index %u", index);
}

/**
 * @brief 在指定索引创建Constant Buffer View
 */
void GlobalDescriptorHeapManager::CreateConstantBufferView(
    ID3D12Device* device,
    const D3D12_CONSTANT_BUFFER_VIEW_DESC* desc,
    uint32_t index)
{
    if (!m_initialized || !m_cbvSrvUavHeap)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateConstantBufferView: not initialized");
        return;
    }

    if (index >= m_cbvSrvUavCapacity)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateConstantBufferView: index %u out of range (capacity: %u)",
                      index, m_cbvSrvUavCapacity);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_cbvSrvUavHeap->GetCPUHandle(index);
    device->CreateConstantBufferView(desc, cpuHandle);

    core::LogInfo("GlobalDescriptorHeapManager",
                 "CreateConstantBufferView: created at index %u", index);
}

/**
 * @brief 在指定索引创建Unordered Access View
 */
void GlobalDescriptorHeapManager::CreateUnorderedAccessView(
    ID3D12Device* device,
    ID3D12Resource* resource,
    ID3D12Resource* counterResource,
    const D3D12_UNORDERED_ACCESS_VIEW_DESC* desc,
    uint32_t index)
{
    if (!m_initialized || !m_cbvSrvUavHeap)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateUnorderedAccessView: not initialized");
        return;
    }

    if (index >= m_cbvSrvUavCapacity)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateUnorderedAccessView: index %u out of range (capacity: %u)",
                      index, m_cbvSrvUavCapacity);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_cbvSrvUavHeap->GetCPUHandle(index);
    device->CreateUnorderedAccessView(resource, counterResource, desc, cpuHandle);

    core::LogInfo("GlobalDescriptorHeapManager",
                 "CreateUnorderedAccessView: created at index %u", index);
}

/**
 * @brief 在指定索引创建Render Target View
 *
 * 教学要点: RTV使用独立的堆，不在全局Bindless CBV_SRV_UAV堆中
 */
void GlobalDescriptorHeapManager::CreateRenderTargetView(
    ID3D12Device* device,
    ID3D12Resource* resource,
    const D3D12_RENDER_TARGET_VIEW_DESC* desc,
    uint32_t index)
{
    if (!m_initialized || !m_rtvHeap)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateRenderTargetView: not initialized");
        return;
    }

    if (index >= m_rtvCapacity)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateRenderTargetView: index %u out of range (capacity: %u)",
                      index, m_rtvCapacity);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_rtvHeap->GetCPUHandle(index);
    device->CreateRenderTargetView(resource, desc, cpuHandle);

    core::LogInfo("GlobalDescriptorHeapManager",
                 "CreateRenderTargetView: created at index %u", index);
}

/**
 * @brief 在指定索引创建Depth Stencil View
 *
 * 教学要点: DSV使用独立的堆，不在全局Bindless CBV_SRV_UAV堆中
 */
void GlobalDescriptorHeapManager::CreateDepthStencilView(
    ID3D12Device* device,
    ID3D12Resource* resource,
    const D3D12_DEPTH_STENCIL_VIEW_DESC* desc,
    uint32_t index)
{
    if (!m_initialized || !m_dsvHeap)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateDepthStencilView: not initialized");
        return;
    }

    if (index >= m_dsvCapacity)
    {
        core::LogError("GlobalDescriptorHeapManager",
                      "CreateDepthStencilView: index %u out of range (capacity: %u)",
                      index, m_dsvCapacity);
        return;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_dsvHeap->GetCPUHandle(index);
    device->CreateDepthStencilView(resource, desc, cpuHandle);

    core::LogInfo("GlobalDescriptorHeapManager",
                 "CreateDepthStencilView: created at index %u", index);
}

/**
 * @brief 获取堆统计信息
 */
GlobalDescriptorHeapManager::HeapStats GlobalDescriptorHeapManager::GetStats() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    HeapStats stats = {};

    // CBV/SRV/UAV堆统计 (Bindless统一大堆)
    stats.cbvSrvUavCapacity  = m_cbvSrvUavCapacity;
    stats.cbvSrvUavUsed      = m_cbvSrvUavHeap ? m_cbvSrvUavHeap->used : 0;
    stats.cbvSrvUavAllocated = m_totalCbvSrvUavAllocated;
    stats.cbvSrvUavPeakUsed  = m_peakCbvSrvUavUsed;

    // RTV堆统计 (渲染目标专用)
    stats.rtvCapacity  = m_rtvCapacity;
    stats.rtvUsed      = m_rtvHeap ? m_rtvHeap->used : 0;
    stats.rtvAllocated = m_totalRtvAllocated;
    stats.rtvPeakUsed  = m_peakRtvUsed;

    // DSV堆统计 (深度模板专用)
    stats.dsvCapacity  = m_dsvCapacity;
    stats.dsvUsed      = m_dsvHeap ? m_dsvHeap->used : 0;
    stats.dsvAllocated = m_totalDsvAllocated;
    stats.dsvPeakUsed  = m_peakDsvUsed;

    // Sampler堆统计
    stats.samplerCapacity  = m_samplerCapacity;
    stats.samplerUsed      = m_samplerHeap ? m_samplerHeap->used : 0;
    stats.samplerAllocated = m_totalSamplerAllocated;
    stats.samplerPeakUsed  = m_peakSamplerUsed;

    // 计算使用率 (0.0-1.0)
    stats.cbvSrvUavUsageRatio = (m_cbvSrvUavCapacity > 0) ? static_cast<float>(stats.cbvSrvUavUsed) / m_cbvSrvUavCapacity : 0.0f;
    stats.rtvUsageRatio       = (m_rtvCapacity > 0) ? static_cast<float>(stats.rtvUsed) / m_rtvCapacity : 0.0f;
    stats.dsvUsageRatio       = (m_dsvCapacity > 0) ? static_cast<float>(stats.dsvUsed) / m_dsvCapacity : 0.0f;
    stats.samplerUsageRatio   = (m_samplerCapacity > 0) ? static_cast<float>(stats.samplerUsed) / m_samplerCapacity : 0.0f;

    return stats;
}

/**
 * @brief 获取CBV/SRV/UAV堆使用率 (Bindless统一大堆)
 */
float GlobalDescriptorHeapManager::GetCbvSrvUavUsageRatio() const
{
    return GetStats().cbvSrvUavUsageRatio;
}

/**
 * @brief 获取RTV堆使用率 (渲染目标专用)
 */
float GlobalDescriptorHeapManager::GetRtvUsageRatio() const
{
    return GetStats().rtvUsageRatio;
}

/**
 * @brief 获取DSV堆使用率 (深度模板专用)
 */
float GlobalDescriptorHeapManager::GetDsvUsageRatio() const
{
    return GetStats().dsvUsageRatio;
}

/**
 * @brief 获取Sampler堆使用率
 */
float GlobalDescriptorHeapManager::GetSamplerUsageRatio() const
{
    return GetStats().samplerUsageRatio;
}

/**
 * @brief 检查是否还有足够空间
 */
bool GlobalDescriptorHeapManager::HasEnoughSpace(uint32_t cbvSrvUavCount, uint32_t rtvCount, uint32_t dsvCount, uint32_t samplerCount) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 检查CBV/SRV/UAV堆空间 (Bindless统一大堆)
    uint32_t cbvSrvUavUsed = m_cbvSrvUavHeap ? m_cbvSrvUavHeap->used : 0;
    bool     cbvSrvUavOk   = (cbvSrvUavUsed + cbvSrvUavCount <= m_cbvSrvUavCapacity);

    // 检查RTV堆空间 (渲染目标专用)
    uint32_t rtvUsed = m_rtvHeap ? m_rtvHeap->used : 0;
    bool     rtvOk   = (rtvUsed + rtvCount <= m_rtvCapacity);

    // 检查DSV堆空间 (深度模板专用)
    uint32_t dsvUsed = m_dsvHeap ? m_dsvHeap->used : 0;
    bool     dsvOk   = (dsvUsed + dsvCount <= m_dsvCapacity);

    // 检查Sampler堆空间
    uint32_t samplerUsed = m_samplerHeap ? m_samplerHeap->used : 0;
    bool     samplerOk   = (samplerUsed + samplerCount <= m_samplerCapacity);

    // 所有堆都有足够空间才返回true
    return cbvSrvUavOk && rtvOk && dsvOk && samplerOk;
}

/**
 * @brief 碎片整理
 */
void GlobalDescriptorHeapManager::DefragmentHeaps()
{
    // TODO: 稍后完成完整实现 - 这是复杂操作，需要重新创建堆
    // 目前为空实现
}

// ==================== 私有方法实现 ====================

/**
 * @brief 创建描述符堆
 */
std::unique_ptr<GlobalDescriptorHeapManager::DescriptorHeap> GlobalDescriptorHeapManager::CreateDescriptorHeap(HeapType type, uint32_t capacity)
{
    auto heap      = std::make_unique<DescriptorHeap>();
    heap->type     = type;
    heap->capacity = capacity;
    heap->used     = 0;

    // 1. 获取D3D12设备
    auto device = D3D12RenderSystem::GetDevice();
    if (!device)
    {
        return nullptr;
    }

    // 2. 设置堆描述符 - 根据堆类型配置不同参数
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors             = capacity;
    desc.NodeMask                   = 0; // 单GPU系统

    // 3. 根据堆类型设置特定属性
    switch (type)
    {
    case HeapType::CBV_SRV_UAV:
        // Bindless统一大堆 - 必须SHADER_VISIBLE
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        break;

    case HeapType::RTV:
        // 渲染目标堆 - 不需要SHADER_VISIBLE
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        break;

    case HeapType::DSV:
        // 深度模板堆 - 不需要SHADER_VISIBLE
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        break;

    case HeapType::Sampler:
        // 采样器堆 - 必须SHADER_VISIBLE
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        break;

    default:
        return nullptr;
    }

    // 4. 创建D3D12描述符堆
    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap->heap));
    if (FAILED(hr))
    {
        return nullptr;
    }

    // 5. 获取描述符大小和起始句柄
    heap->descriptorSize = device->GetDescriptorHandleIncrementSize(desc.Type);
    heap->cpuStart       = heap->heap->GetCPUDescriptorHandleForHeapStart();

    // 6. 获取GPU句柄（仅对SHADER_VISIBLE堆有效）
    if (desc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    {
        heap->gpuStart = heap->heap->GetGPUDescriptorHandleForHeapStart();
    }
    else
    {
        // 非着色器可见堆的GPU句柄无效
        heap->gpuStart = {};
    }

    // 7. 设置调试名称（便于调试）
    const wchar_t* debugName = L"Unknown";
    switch (type)
    {
    case HeapType::CBV_SRV_UAV: debugName = L"Enigma_CBV_SRV_UAV_Heap";
        break;
    case HeapType::RTV: debugName = L"Enigma_RTV_Heap";
        break;
    case HeapType::DSV: debugName = L"Enigma_DSV_Heap";
        break;
    case HeapType::Sampler: debugName = L"Enigma_Sampler_Heap";
        break;
    }
    heap->heap->SetName(debugName);

    return heap;
}

/**
 * @brief 分配指定类型的索引
 */
uint32_t GlobalDescriptorHeapManager::AllocateIndex(HeapType heapType)
{
    // 1. 根据堆类型选择对应的使用状态数组和容量
    std::vector<bool>* usedArray     = nullptr;
    uint32_t*          nextFreeIndex = nullptr;
    uint32_t           capacity      = 0;

    switch (heapType)
    {
    case HeapType::CBV_SRV_UAV:
        usedArray = &m_cbvSrvUavUsed;
        nextFreeIndex = &m_nextFreeCbvSrvUav;
        capacity      = m_cbvSrvUavCapacity;
        break;
    case HeapType::RTV:
        usedArray = &m_rtvUsed;
        nextFreeIndex = &m_nextFreeRtv;
        capacity      = m_rtvCapacity;
        break;
    case HeapType::DSV:
        usedArray = &m_dsvUsed;
        nextFreeIndex = &m_nextFreeDsv;
        capacity      = m_dsvCapacity;
        break;
    case HeapType::Sampler:
        usedArray = &m_samplerUsed;
        nextFreeIndex = &m_nextFreeSampler;
        capacity      = m_samplerCapacity;
        break;
    default:
        return UINT32_MAX;
    }

    // 2. 验证数组和容量有效性
    if (!usedArray || capacity == 0 || usedArray->size() != capacity)
    {
        return UINT32_MAX;
    }

    // 3. 从nextFreeIndex开始搜索空闲位置 (性能优化)
    for (uint32_t i = *nextFreeIndex; i < capacity; ++i)
    {
        if (!(*usedArray)[i])
        {
            (*usedArray)[i] = true;
            *nextFreeIndex  = i + 1; // 下次从后面开始搜索
            UpdateStats(heapType, +1);
            return i;
        }
    }

    // 4. 如果从nextFreeIndex没找到，从头开始搜索
    for (uint32_t i = 0; i < *nextFreeIndex && i < capacity; ++i)
    {
        if (!(*usedArray)[i])
        {
            (*usedArray)[i] = true;
            *nextFreeIndex  = i + 1;
            UpdateStats(heapType, +1);
            return i;
        }
    }

    // 5. 没有空闲位置，分配失败
    return UINT32_MAX;
}

/**
 * @brief 释放指定类型的索引
 */
void GlobalDescriptorHeapManager::FreeIndex(HeapType heapType, uint32_t index)
{
    // 1. 根据堆类型选择对应的使用状态数组和容量
    std::vector<bool>* usedArray     = nullptr;
    uint32_t*          nextFreeIndex = nullptr;
    uint32_t           capacity      = 0;

    switch (heapType)
    {
    case HeapType::CBV_SRV_UAV:
        usedArray = &m_cbvSrvUavUsed;
        nextFreeIndex = &m_nextFreeCbvSrvUav;
        capacity      = m_cbvSrvUavCapacity;
        break;
    case HeapType::RTV:
        usedArray = &m_rtvUsed;
        nextFreeIndex = &m_nextFreeRtv;
        capacity      = m_rtvCapacity;
        break;
    case HeapType::DSV:
        usedArray = &m_dsvUsed;
        nextFreeIndex = &m_nextFreeDsv;
        capacity      = m_dsvCapacity;
        break;
    case HeapType::Sampler:
        usedArray = &m_samplerUsed;
        nextFreeIndex = &m_nextFreeSampler;
        capacity      = m_samplerCapacity;
        break;
    default:
        return; // 无效堆类型，直接返回
    }

    // 2. 验证索引有效性
    if (!usedArray || index >= capacity || index >= usedArray->size())
    {
        return; // 索引无效，直接返回
    }

    // 3. 检查索引是否已分配
    if (!(*usedArray)[index])
    {
        return; // 索引未分配，直接返回
    }

    // 4. 释放索引
    (*usedArray)[index] = false;

    // 5. 优化下次分配的起始位置
    if (index < *nextFreeIndex)
    {
        *nextFreeIndex = index;
    }

    // 6. 更新统计信息
    UpdateStats(heapType, -1);
}

/**
 * @brief 查询硬件支持的最大描述符数量
 */
uint32_t GlobalDescriptorHeapManager::QueryMaxDescriptorCount(HeapType heapType) const
{
    // TODO: 稍后完成完整实现
    // DirectX 12的典型限制
    return (heapType == HeapType::CBV_SRV_UAV) ? 1000000 : 2048;
}

/**
 * @brief 更新使用统计
 */
void GlobalDescriptorHeapManager::UpdateStats(HeapType heapType, int32_t delta)
{
#undef max
    switch (heapType)
    {
    case HeapType::CBV_SRV_UAV:
        if (m_cbvSrvUavHeap)
        {
            m_cbvSrvUavHeap->used += delta;
            m_totalCbvSrvUavAllocated += delta;
            if (delta > 0)
            {
                m_peakCbvSrvUavUsed = std::max(m_peakCbvSrvUavUsed, m_cbvSrvUavHeap->used);
            }
        }
        break;

    case HeapType::RTV:
        if (m_rtvHeap)
        {
            m_rtvHeap->used += delta;
            m_totalRtvAllocated += delta;
            if (delta > 0)
            {
                m_peakRtvUsed = std::max(m_peakRtvUsed, m_rtvHeap->used);
            }
        }
        break;

    case HeapType::DSV:
        if (m_dsvHeap)
        {
            m_dsvHeap->used += delta;
            m_totalDsvAllocated += delta;
            if (delta > 0)
            {
                m_peakDsvUsed = std::max(m_peakDsvUsed, m_dsvHeap->used);
            }
        }
        break;

    case HeapType::Sampler:
        if (m_samplerHeap)
        {
            m_samplerHeap->used += delta;
            m_totalSamplerAllocated += delta;
            if (delta > 0)
            {
                m_peakSamplerUsed = std::max(m_peakSamplerUsed, m_samplerHeap->used);
            }
        }
        break;

    default:
        break;
    }
}

// ========================================================================
// 描述符堆容量和数量查询方法实现 (BindlessResourceManager依赖)
// ========================================================================

/**
 * @brief 获取CBV/SRV/UAV堆已使用数量
 */
uint32_t GlobalDescriptorHeapManager::GetCbvSrvUavCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_cbvSrvUavHeap ? m_cbvSrvUavHeap->used : 0;
}

/**
 * @brief 获取CBV/SRV/UAV堆总容量
 */
uint32_t GlobalDescriptorHeapManager::GetCbvSrvUavCapacity() const
{
    return m_cbvSrvUavCapacity;
}

/**
 * @brief 获取RTV堆已使用数量
 */
uint32_t GlobalDescriptorHeapManager::GetRtvCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_rtvHeap ? m_rtvHeap->used : 0;
}

/**
 * @brief 获取RTV堆总容量
 */
uint32_t GlobalDescriptorHeapManager::GetRtvCapacity() const
{
    return m_rtvCapacity;
}

/**
 * @brief 获取DSV堆已使用数量
 */
uint32_t GlobalDescriptorHeapManager::GetDsvCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dsvHeap ? m_dsvHeap->used : 0;
}

/**
 * @brief 获取DSV堆总容量
 */
uint32_t GlobalDescriptorHeapManager::GetDsvCapacity() const
{
    return m_dsvCapacity;
}

/**
 * @brief 获取Sampler堆已使用数量
 */
uint32_t GlobalDescriptorHeapManager::GetSamplerCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_samplerHeap ? m_samplerHeap->used : 0;
}

/**
 * @brief 获取Sampler堆总容量
 */
uint32_t GlobalDescriptorHeapManager::GetSamplerCapacity() const
{
    return m_samplerCapacity;
}
