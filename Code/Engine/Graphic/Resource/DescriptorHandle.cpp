#include "DescriptorHandle.hpp"
#include <sstream>
#include "Engine/Core/Logger/LoggerAPI.hpp"

using namespace enigma::graphic;

// ==================== 构造函数和析构函数 ====================

DescriptorHandle::DescriptorHandle() noexcept
    : m_allocation{}, m_heapManager{}, m_ownsResource(false)
{
    // 教学注释: 默认构造函数创建无效句柄
    // 所有成员都初始化为默认值，不拥有任何资源
}

DescriptorHandle::DescriptorHandle(const GlobalDescriptorHeapManager::DescriptorAllocation& allocation, std::shared_ptr<GlobalDescriptorHeapManager> heapManager) noexcept
    : m_allocation(allocation)
      , m_heapManager(heapManager)
      , m_ownsResource(true)
{
    // 教学注释: 创建时默认拥有资源所有权
    // 析构时会自动调用堆管理器的释放方法
}

DescriptorHandle::~DescriptorHandle()
{
    // 教学注释: 析构时自动释放拥有的描述符资源
    // SafeRelease() 会检查所有权，并正确释放到对应的描述符堆
    SafeRelease();
}

// ==================== 移动语义 ====================

DescriptorHandle::DescriptorHandle(DescriptorHandle&& other) noexcept
    : m_allocation(std::move(other.m_allocation))
      , m_heapManager(std::move(other.m_heapManager))
      , m_ownsResource(other.m_ownsResource)
{
    // 教学注释: 移动构造函数 - 转移资源所有权
    // 重置源对象的所有权标记，防止重复释放
    other.m_ownsResource = false;
}

DescriptorHandle& DescriptorHandle::operator=(DescriptorHandle&& other) noexcept
{
    if (this != &other)
    {
        // 教学注释: 移动赋值运算符 - 先释放当前资源，再转移新资源
        // 1. SafeRelease() 释放当前拥有的描述符(如果有)
        // 2. 然后转移源对象的资源所有权
        SafeRelease();

        m_allocation         = std::move(other.m_allocation);
        m_heapManager        = std::move(other.m_heapManager);
        m_ownsResource       = other.m_ownsResource;
        other.m_ownsResource = false;
    }
    return *this;
}

// ==================== 资源访问接口 ====================

/**
 * @brief 获取CPU描述符句柄
 */
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHandle::GetCpuHandle() const noexcept
{
    return m_allocation.cpuHandle;
}

/**
 * @brief 获取GPU描述符句柄
 */
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHandle::GetGpuHandle() const noexcept
{
    return m_allocation.gpuHandle;
}

/**
 * @brief 获取堆内索引
 */
uint32_t DescriptorHandle::GetHeapIndex() const noexcept
{
    return m_allocation.heapIndex;
}

/**
 * @brief 获取堆类型
 */
GlobalDescriptorHeapManager::HeapType DescriptorHandle::GetHeapType() const noexcept
{
    return m_allocation.heapType;
}

// ==================== 状态查询接口 ====================

/**
 * @brief 检查句柄是否有效
 */
bool DescriptorHandle::IsValid() const noexcept
{
    return m_allocation.isValid && m_allocation.cpuHandle.ptr != 0;
}

/**
 * @brief 检查是否拥有资源
 */
bool DescriptorHandle::OwnsResource() const noexcept
{
    return m_ownsResource;
}

/**
 * @brief 检查堆管理器是否仍然存在
 */
bool DescriptorHandle::IsHeapManagerAlive() const noexcept
{
    return !m_heapManager.expired();
}

// ==================== 资源管理接口 ====================

/**
 * @brief 释放描述符(提前释放)
 */
void DescriptorHandle::Release() noexcept
{
    SafeRelease();
}

/**
 * @brief 重置句柄为无效状态
 */
void DescriptorHandle::Reset() noexcept
{
    m_allocation.Reset();
    m_heapManager.reset();
    m_ownsResource = false;
}

/**
 * @brief 分离资源所有权
 */
GlobalDescriptorHeapManager::DescriptorAllocation DescriptorHandle::Detach() noexcept
{
    auto allocation = m_allocation;
    Reset();
    return allocation;
}

// ==================== 比较操作符 ====================

bool DescriptorHandle::operator==(const DescriptorHandle& other) const noexcept
{
    return m_allocation.cpuHandle.ptr == other.m_allocation.cpuHandle.ptr &&
        m_allocation.heapIndex == other.m_allocation.heapIndex;
}

bool DescriptorHandle::operator!=(const DescriptorHandle& other) const noexcept
{
    return !(*this == other);
}

bool DescriptorHandle::operator<(const DescriptorHandle& other) const noexcept
{
    if (m_allocation.heapType != other.m_allocation.heapType)
    {
        return static_cast<int>(m_allocation.heapType) < static_cast<int>(other.m_allocation.heapType);
    }
    return m_allocation.heapIndex < other.m_allocation.heapIndex;
}

// ==================== 转换操作符 ====================

DescriptorHandle::operator bool() const noexcept
{
    return IsValid();
}

// ==================== 调试和诊断接口 ====================

std::string DescriptorHandle::GetDebugInfo() const
{
    std::ostringstream oss;
    oss << "DescriptorHandle[\n";
    oss << "  IsValid: " << (IsValid() ? "true" : "false") << "\n";
    oss << "  OwnsResource: " << (m_ownsResource ? "true" : "false") << "\n";
    oss << "  HeapManagerAlive: " << (IsHeapManagerAlive() ? "true" : "false") << "\n";
    oss << "  HeapIndex: " << m_allocation.heapIndex << "\n";
    oss << "  CPU Handle: 0x" << std::hex << m_allocation.cpuHandle.ptr << std::dec << "\n";
    oss << "  GPU Handle: 0x" << std::hex << m_allocation.gpuHandle.ptr << std::dec << "\n";
    oss << "  HeapType: " << static_cast<int>(m_allocation.heapType) << "\n";
    oss << "]";
    return oss.str();
}

bool DescriptorHandle::ValidateIntegrity() const noexcept
{
    if (m_allocation.isValid && m_allocation.cpuHandle.ptr == 0)
        return false;
    if (m_allocation.isValid && m_allocation.heapIndex == INVALID_INDEX)
        return false;
    if (m_ownsResource && m_heapManager.expired())
        return false;
    return true;
}

// ==================== 静态工厂方法 ====================

DescriptorHandle DescriptorHandle::CreateInvalid() noexcept
{
    return DescriptorHandle();
}

DescriptorHandle DescriptorHandle::CreateNonOwning(
    const GlobalDescriptorHeapManager::DescriptorAllocation& allocation) noexcept
{
    auto handle           = DescriptorHandle();
    handle.m_allocation   = allocation;
    handle.m_ownsResource = false;
    return handle;
}

// ==================== 私有辅助方法 ====================

void DescriptorHandle::SafeRelease() noexcept
{
    if (!m_ownsResource || !m_allocation.isValid)
        return;

    auto heapManager = m_heapManager.lock();
    if (!heapManager)
    {
        m_allocation.Reset();
        m_ownsResource = false;
        return;
    }

    switch (m_allocation.heapType)
    {
    case GlobalDescriptorHeapManager::HeapType::CBV_SRV_UAV:
        heapManager->FreeCbvSrvUav(m_allocation);
        break;
    case GlobalDescriptorHeapManager::HeapType::RTV:
        heapManager->FreeRtv(m_allocation);
        break;
    case GlobalDescriptorHeapManager::HeapType::DSV:
        heapManager->FreeDsv(m_allocation);
        break;
    case GlobalDescriptorHeapManager::HeapType::Sampler:
        heapManager->FreeSampler(m_allocation);
        break;
    default:
        enigma::core::LogError("DescriptorHandle", "SafeRelease: Unknown heap type %d",
                               static_cast<int>(m_allocation.heapType));
        break;
    }

    m_allocation.Reset();
    m_ownsResource = false;
}

void DescriptorHandle::MoveFrom(DescriptorHandle&& other) noexcept
{
    m_allocation   = std::move(other.m_allocation);
    m_heapManager  = std::move(other.m_heapManager);
    m_ownsResource = other.m_ownsResource;

    other.m_allocation.Reset();
    other.m_heapManager.reset();
    other.m_ownsResource = false;
}

void DescriptorHandle::Swap(DescriptorHandle& other) noexcept
{
    using std::swap;
    swap(m_allocation, other.m_allocation);
    swap(m_heapManager, other.m_heapManager);
    swap(m_ownsResource, other.m_ownsResource);
}

// ==================== 非成员函数 ====================

void enigma::graphic::swap(DescriptorHandle& lhs, DescriptorHandle& rhs) noexcept
{
    lhs.Swap(rhs);
}
