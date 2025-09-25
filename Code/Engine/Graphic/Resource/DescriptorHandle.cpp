#include "DescriptorHandle.hpp"

using namespace enigma::graphic;

// ==================== 构造函数和析构函数 ====================

DescriptorHandle::DescriptorHandle() noexcept
    : m_allocation{}, m_heapManager{}, m_ownsResource(false)
{
    // TODO: 稍后完成完整实现
}

DescriptorHandle::~DescriptorHandle()
{
    // TODO: 稍后完成完整实现 - 释放描述符资源
}

// ==================== 移动语义 ====================

DescriptorHandle::DescriptorHandle(DescriptorHandle&& other) noexcept
    : m_allocation(std::move(other.m_allocation))
    , m_heapManager(std::move(other.m_heapManager))
    , m_ownsResource(other.m_ownsResource)
{
    // TODO: 稍后完成完整实现 - 重置源对象
    other.m_ownsResource = false;
}

DescriptorHandle& DescriptorHandle::operator=(DescriptorHandle&& other) noexcept
{
    if (this != &other) {
        // TODO: 稍后完成完整实现 - 先释放当前资源，再转移新资源
        m_allocation = std::move(other.m_allocation);
        m_heapManager = std::move(other.m_heapManager);
        m_ownsResource = other.m_ownsResource;
        other.m_ownsResource = false;
    }
    return *this;
}