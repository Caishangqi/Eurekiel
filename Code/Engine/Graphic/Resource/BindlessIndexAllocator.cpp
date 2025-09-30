#include "BindlessIndexAllocator.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // BindlessIndexAllocator 主要实现
    // ========================================================================

    BindlessIndexAllocator::BindlessIndexAllocator()
        : m_allocatedTextureCount(0)
        , m_allocatedBufferCount(0)
    {
        // 初始化纹理索引位图 (1,000,000个索引)
        m_textureIndexUsed.resize(TEXTURE_INDEX_END - TEXTURE_INDEX_START + 1, false);

        // 初始化缓冲区索引位图 (1,000,000个索引)
        m_bufferIndexUsed.resize(BUFFER_INDEX_END - BUFFER_INDEX_START + 1, false);

        enigma::core::LogInfo("BindlessIndexAllocator",
                     "Initialized: Texture capacity=%u, Buffer capacity=%u",
                     static_cast<uint32_t>(m_textureIndexUsed.size()),
                     static_cast<uint32_t>(m_bufferIndexUsed.size()));
    }

    uint32_t BindlessIndexAllocator::AllocateTextureIndex()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 1. 查找空闲索引
        uint32_t index = FindFreeTextureIndex();

        if (index == INVALID_INDEX)
        {
            enigma::core::LogError("BindlessIndexAllocator",
                          "AllocateTextureIndex: no available texture index (1M limit reached)");
            return INVALID_INDEX;
        }

        // 2. 标记为已使用
        m_textureIndexUsed[index] = true;
        m_allocatedTextureCount++;

        enigma::core::LogInfo("BindlessIndexAllocator",
                     "AllocateTextureIndex: allocated index %u, total: %u",
                     index, m_allocatedTextureCount);

        return index;
    }

    uint32_t BindlessIndexAllocator::AllocateBufferIndex()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 1. 查找空闲索引
        uint32_t index = FindFreeBufferIndex();

        if (index == INVALID_INDEX)
        {
            enigma::core::LogError("BindlessIndexAllocator",
                          "AllocateBufferIndex: no available buffer index (1M limit reached)");
            return INVALID_INDEX;
        }

        // 2. 标记为已使用
        uint32_t arrayIndex = index - BUFFER_INDEX_START;
        m_bufferIndexUsed[arrayIndex] = true;
        m_allocatedBufferCount++;

        enigma::core::LogInfo("BindlessIndexAllocator",
                     "AllocateBufferIndex: allocated index %u, total: %u",
                     index, m_allocatedBufferCount);

        return index;
    }

    bool BindlessIndexAllocator::FreeTextureIndex(uint32_t index)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 1. 验证索引范围
        if (index < TEXTURE_INDEX_START || index > TEXTURE_INDEX_END)
        {
            enigma::core::LogError("BindlessIndexAllocator",
                          "FreeTextureIndex: invalid index %u (valid range: %u-%u)",
                          index, TEXTURE_INDEX_START, TEXTURE_INDEX_END);
            return false;
        }

        // 2. 检查是否已分配
        if (!m_textureIndexUsed[index])
        {
            enigma::core::LogWarn("BindlessIndexAllocator",
                            "FreeTextureIndex: index %u not allocated (double free?)",
                            index);
            return false;
        }

        // 3. 释放索引
        m_textureIndexUsed[index] = false;
        m_allocatedTextureCount--;

        enigma::core::LogInfo("BindlessIndexAllocator",
                     "FreeTextureIndex: freed index %u, remaining: %u",
                     index, m_allocatedTextureCount);

        return true;
    }

    bool BindlessIndexAllocator::FreeBufferIndex(uint32_t index)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 1. 验证索引范围
        if (index < BUFFER_INDEX_START || index > BUFFER_INDEX_END)
        {
            enigma::core::LogError("BindlessIndexAllocator",
                          "FreeBufferIndex: invalid index %u (valid range: %u-%u)",
                          index, BUFFER_INDEX_START, BUFFER_INDEX_END);
            return false;
        }

        // 2. 转换为数组索引
        uint32_t arrayIndex = index - BUFFER_INDEX_START;

        // 3. 检查是否已分配
        if (!m_bufferIndexUsed[arrayIndex])
        {
            enigma::core::LogWarn("BindlessIndexAllocator",
                            "FreeBufferIndex: index %u not allocated (double free?)",
                            index);
            return false;
        }

        // 4. 释放索引
        m_bufferIndexUsed[arrayIndex] = false;
        m_allocatedBufferCount--;

        enigma::core::LogInfo("BindlessIndexAllocator",
                     "FreeBufferIndex: freed index %u, remaining: %u",
                     index, m_allocatedBufferCount);

        return true;
    }

    uint32_t BindlessIndexAllocator::GetAllocatedTextureCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_allocatedTextureCount;
    }

    uint32_t BindlessIndexAllocator::GetAllocatedBufferCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_allocatedBufferCount;
    }

    uint32_t BindlessIndexAllocator::GetAvailableTextureCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_textureIndexUsed.size()) - m_allocatedTextureCount;
    }

    uint32_t BindlessIndexAllocator::GetAvailableBufferCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_bufferIndexUsed.size()) - m_allocatedBufferCount;
    }

    void BindlessIndexAllocator::Reset()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        // 重置所有位图
        std::fill(m_textureIndexUsed.begin(), m_textureIndexUsed.end(), false);
        std::fill(m_bufferIndexUsed.begin(), m_bufferIndexUsed.end(), false);

        enigma::core::LogInfo("BindlessIndexAllocator",
                     "Reset: freed %u textures and %u buffers",
                     m_allocatedTextureCount, m_allocatedBufferCount);

        m_allocatedTextureCount = 0;
        m_allocatedBufferCount = 0;
    }

    // ========================================================================
    // 私有辅助方法
    // ========================================================================

    uint32_t BindlessIndexAllocator::FindFreeTextureIndex()
    {
        // 简单线性搜索 (可优化为位扫描算法)
        for (uint32_t i = 0; i < m_textureIndexUsed.size(); ++i)
        {
            if (!m_textureIndexUsed[i])
            {
                return i + TEXTURE_INDEX_START;
            }
        }
        return INVALID_INDEX;
    }

    uint32_t BindlessIndexAllocator::FindFreeBufferIndex()
    {
        // 简单线性搜索 (可优化为位扫描算法)
        for (uint32_t i = 0; i < m_bufferIndexUsed.size(); ++i)
        {
            if (!m_bufferIndexUsed[i])
            {
                return i + BUFFER_INDEX_START;
            }
        }
        return INVALID_INDEX;
    }

} // namespace enigma::graphic
