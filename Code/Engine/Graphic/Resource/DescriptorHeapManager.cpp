#include "DescriptorHeapManager.hpp"
#include <cassert>
#include <algorithm>

using namespace enigma::graphic;

// ==================== DescriptorAllocation 实现 ====================

/**
 * @brief DescriptorAllocation默认构造函数
 *
 * 教学要点: 初始化所有成员为无效状态，确保对象处于明确的初始状态
 */
DescriptorHeapManager::DescriptorAllocation::DescriptorAllocation()
    : cpuHandle{}      // 零初始化CPU句柄
    , gpuHandle{}      // 零初始化GPU句柄
    , heapIndex(UINT32_MAX)  // 无效索引
    , heapType(HeapType::CBV_SRV_UAV)  // 默认堆类型
    , isValid(false)   // 标记为无效
{
    // 教学注释: 使用UINT32_MAX作为无效索引是常见做法
    // 这样可以很容易地检测未初始化或无效的分配
}

/**
 * @brief 重置分配状态为无效
 */
void DescriptorHeapManager::DescriptorAllocation::Reset()
{
    cpuHandle = {};
    gpuHandle = {};
    heapIndex = UINT32_MAX;
    heapType = HeapType::CBV_SRV_UAV;
    isValid = false;
}

// ==================== DescriptorHeap 实现 ====================

/**
 * @brief DescriptorHeap默认构造函数
 */
DescriptorHeapManager::DescriptorHeap::DescriptorHeap()
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
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeapManager::DescriptorHeap::GetCPUHandle(uint32_t index) const
{
    // TODO: 稍后完成完整实现
    D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuStart;
    handle.ptr += index * descriptorSize;
    return handle;
}

/**
 * @brief 获取指定索引的GPU句柄
 */
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeapManager::DescriptorHeap::GetGPUHandle(uint32_t index) const
{
    // TODO: 稍后完成完整实现
    D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuStart;
    handle.ptr += index * descriptorSize;
    return handle;
}

// ==================== DescriptorHeapManager 实现 ====================

/**
 * @brief DescriptorHeapManager构造函数
 */
DescriptorHeapManager::DescriptorHeapManager()
    : m_cbvSrvUavHeap(nullptr)
    , m_samplerHeap(nullptr)
    , m_cbvSrvUavCapacity(0)
    , m_samplerCapacity(0)
    , m_nextFreeCbvSrvUav(0)
    , m_nextFreeSampler(0)
    , m_totalCbvSrvUavAllocated(0)
    , m_totalSamplerAllocated(0)
    , m_peakCbvSrvUavUsed(0)
    , m_peakSamplerUsed(0)
    , m_initialized(false)
{
    // TODO: 稍后完成完整实现
}

/**
 * @brief DescriptorHeapManager析构函数
 */
DescriptorHeapManager::~DescriptorHeapManager()
{
    // TODO: 稍后完成完整实现 - 调用Shutdown清理资源
    Shutdown();
}

/**
 * @brief 初始化描述符堆管理器
 */
bool DescriptorHeapManager::Initialize(uint32_t cbvSrvUavCapacity, uint32_t samplerCapacity)
{
    // 避免未引用参数警告
    (void)cbvSrvUavCapacity;
    (void)samplerCapacity;

    // TODO: 稍后完成完整实现
    // 目前返回false表示未实现
    return false;
}

/**
 * @brief 释放所有资源
 */
void DescriptorHeapManager::Shutdown()
{
    // TODO: 稍后完成完整实现
    std::lock_guard<std::mutex> lock(m_mutex);

    m_cbvSrvUavHeap.reset();
    m_samplerHeap.reset();
    m_cbvSrvUavUsed.clear();
    m_samplerUsed.clear();

    m_initialized = false;
}

/**
 * @brief 检查是否已初始化
 */
bool DescriptorHeapManager::IsInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_initialized;
}

/**
 * @brief 分配CBV/SRV/UAV描述符
 */
DescriptorHeapManager::DescriptorAllocation DescriptorHeapManager::AllocateCbvSrvUav()
{
    // TODO: 稍后完成完整实现
    DescriptorAllocation allocation;
    // 目前返回无效分配
    return allocation;
}

/**
 * @brief 分配Sampler描述符
 */
DescriptorHeapManager::DescriptorAllocation DescriptorHeapManager::AllocateSampler()
{
    // TODO: 稍后完成完整实现
    DescriptorAllocation allocation;
    // 目前返回无效分配
    return allocation;
}

/**
 * @brief 批量分配CBV/SRV/UAV描述符
 */
std::vector<DescriptorHeapManager::DescriptorAllocation> DescriptorHeapManager::BatchAllocateCbvSrvUav(uint32_t count)
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
 */
bool DescriptorHeapManager::FreeCbvSrvUav(const DescriptorAllocation& allocation)
{
    // 避免未引用参数警告
    (void)allocation;

    // TODO: 稍后完成完整实现
    return false;
}

/**
 * @brief 释放Sampler描述符
 */
bool DescriptorHeapManager::FreeSampler(const DescriptorAllocation& allocation)
{
    // 避免未引用参数警告
    (void)allocation;

    // TODO: 稍后完成完整实现
    return false;
}

/**
 * @brief 获取CBV/SRV/UAV描述符堆
 */
ID3D12DescriptorHeap* DescriptorHeapManager::GetCbvSrvUavHeap() const
{
    // TODO: 稍后完成完整实现
    return m_cbvSrvUavHeap ? m_cbvSrvUavHeap->heap.Get() : nullptr;
}

/**
 * @brief 获取Sampler描述符堆
 */
ID3D12DescriptorHeap* DescriptorHeapManager::GetSamplerHeap() const
{
    // TODO: 稍后完成完整实现
    return m_samplerHeap ? m_samplerHeap->heap.Get() : nullptr;
}

/**
 * @brief 设置描述符堆到命令列表
 */
void DescriptorHeapManager::SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList) const
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

/**
 * @brief 获取堆统计信息
 */
DescriptorHeapManager::HeapStats DescriptorHeapManager::GetStats() const
{
    // TODO: 稍后完成完整实现
    std::lock_guard<std::mutex> lock(m_mutex);

    HeapStats stats = {};
    stats.cbvSrvUavCapacity = m_cbvSrvUavCapacity;
    stats.cbvSrvUavUsed = m_cbvSrvUavHeap ? m_cbvSrvUavHeap->used : 0;
    stats.cbvSrvUavAllocated = m_totalCbvSrvUavAllocated;
    stats.cbvSrvUavPeakUsed = m_peakCbvSrvUavUsed;

    stats.samplerCapacity = m_samplerCapacity;
    stats.samplerUsed = m_samplerHeap ? m_samplerHeap->used : 0;
    stats.samplerAllocated = m_totalSamplerAllocated;
    stats.samplerPeakUsed = m_peakSamplerUsed;

    stats.cbvSrvUavUsageRatio = (m_cbvSrvUavCapacity > 0) ?
        static_cast<float>(stats.cbvSrvUavUsed) / m_cbvSrvUavCapacity : 0.0f;
    stats.samplerUsageRatio = (m_samplerCapacity > 0) ?
        static_cast<float>(stats.samplerUsed) / m_samplerCapacity : 0.0f;

    return stats;
}

/**
 * @brief 获取CBV/SRV/UAV堆使用率
 */
float DescriptorHeapManager::GetCbvSrvUavUsageRatio() const
{
    // TODO: 稍后完成完整实现
    return GetStats().cbvSrvUavUsageRatio;
}

/**
 * @brief 获取Sampler堆使用率
 */
float DescriptorHeapManager::GetSamplerUsageRatio() const
{
    // TODO: 稍后完成完整实现
    return GetStats().samplerUsageRatio;
}

/**
 * @brief 检查是否还有足够空间
 */
bool DescriptorHeapManager::HasEnoughSpace(uint32_t cbvSrvUavCount, uint32_t samplerCount) const
{
    // TODO: 稍后完成完整实现
    std::lock_guard<std::mutex> lock(m_mutex);

    uint32_t cbvSrvUavUsed = m_cbvSrvUavHeap ? m_cbvSrvUavHeap->used : 0;
    uint32_t samplerUsed = m_samplerHeap ? m_samplerHeap->used : 0;

    return (cbvSrvUavUsed + cbvSrvUavCount <= m_cbvSrvUavCapacity) &&
           (samplerUsed + samplerCount <= m_samplerCapacity);
}

/**
 * @brief 碎片整理
 */
void DescriptorHeapManager::DefragmentHeaps()
{
    // TODO: 稍后完成完整实现 - 这是复杂操作，需要重新创建堆
    // 目前为空实现
}

// ==================== 私有方法实现 ====================

/**
 * @brief 创建描述符堆
 */
std::unique_ptr<DescriptorHeapManager::DescriptorHeap> DescriptorHeapManager::CreateDescriptorHeap(HeapType type, uint32_t capacity)
{
    // TODO: 稍后完成完整实现
    auto heap = std::make_unique<DescriptorHeap>();
    heap->type = type;
    heap->capacity = capacity;
    heap->used = 0;

    // 目前返回基本初始化的堆，具体的DirectX创建逻辑稍后实现
    return heap;
}

/**
 * @brief 分配指定类型的索引
 */
uint32_t DescriptorHeapManager::AllocateIndex(HeapType heapType)
{
    // 避免未引用参数警告
    (void)heapType;

    // TODO: 稍后完成完整实现
    return UINT32_MAX;  // 表示分配失败
}

/**
 * @brief 释放指定类型的索引
 */
void DescriptorHeapManager::FreeIndex(HeapType heapType, uint32_t index)
{
    // 避免未引用参数警告
    (void)heapType;
    (void)index;

    // TODO: 稍后完成完整实现
}

/**
 * @brief 查询硬件支持的最大描述符数量
 */
uint32_t DescriptorHeapManager::QueryMaxDescriptorCount(HeapType heapType) const
{
    // TODO: 稍后完成完整实现
    // DirectX 12的典型限制
    return (heapType == HeapType::CBV_SRV_UAV) ? 1000000 : 2048;
}

/**
 * @brief 更新使用统计
 */
void DescriptorHeapManager::UpdateStats(HeapType heapType, int32_t delta)
{
#undef max
    // TODO: 稍后完成完整实现
    if (heapType == HeapType::CBV_SRV_UAV)
    {
        m_totalCbvSrvUavAllocated += delta;
        if (delta > 0 && m_cbvSrvUavHeap)
        {
            m_peakCbvSrvUavUsed = std::max(m_peakCbvSrvUavUsed, m_cbvSrvUavHeap->used);
        }
    }
    else
    {
        m_totalSamplerAllocated += delta;
        if (delta > 0 && m_samplerHeap)
        {
            m_peakSamplerUsed = std::max(m_peakSamplerUsed, m_samplerHeap->used);
        }
    }
}