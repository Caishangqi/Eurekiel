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
        // ====================================================================
        // FreeList初始化 - 预分配所有索引
        // ====================================================================

        // 1. 纹理FreeList: 初始化为 [0, 1, 2, ..., 999999]
        constexpr uint32_t textureCapacity = TEXTURE_INDEX_END - TEXTURE_INDEX_START + 1;
        m_textureFreeList.reserve(textureCapacity);
        for (uint32_t i = 0; i < textureCapacity; ++i)
        {
            m_textureFreeList.push_back(i + TEXTURE_INDEX_START);
        }

        // 2. 缓冲区FreeList: 初始化为 [1000000, 1000001, ..., 1999999]
        constexpr uint32_t bufferCapacity = BUFFER_INDEX_END - BUFFER_INDEX_START + 1;
        m_bufferFreeList.reserve(bufferCapacity);
        for (uint32_t i = 0; i < bufferCapacity; ++i)
        {
            m_bufferFreeList.push_back(i + BUFFER_INDEX_START);
        }

        enigma::core::LogInfo("BindlessIndexAllocator",
                              "Initialized (FreeList): Texture capacity=%u, Buffer capacity=%u",
                              textureCapacity, bufferCapacity);
    }

    uint32_t BindlessIndexAllocator::AllocateTextureIndex()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return AllocateTextureIndexInternal();
    }

    uint32_t BindlessIndexAllocator::AllocateBufferIndex()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return AllocateBufferIndexInternal();
    }

    bool BindlessIndexAllocator::FreeTextureIndex(uint32_t index)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return FreeTextureIndexInternal(index);
    }

    bool BindlessIndexAllocator::FreeBufferIndex(uint32_t index)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return FreeBufferIndexInternal(index);
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
        return static_cast<uint32_t>(m_textureFreeList.size());
    }

    uint32_t BindlessIndexAllocator::GetAvailableBufferCount() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_bufferFreeList.size());
    }

    void BindlessIndexAllocator::Reset()
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        enigma::core::LogInfo("BindlessIndexAllocator",
                              "Reset: freeing %u textures and %u buffers",
                              m_allocatedTextureCount, m_allocatedBufferCount);

        // ====================================================================
        // 重新初始化FreeList
        // ====================================================================

        // 1. 清空并重建纹理FreeList
        m_textureFreeList.clear();
        constexpr uint32_t textureCapacity = TEXTURE_INDEX_END - TEXTURE_INDEX_START + 1;
        m_textureFreeList.reserve(textureCapacity);
        for (uint32_t i = 0; i < textureCapacity; ++i)
        {
            m_textureFreeList.push_back(i + TEXTURE_INDEX_START);
        }

        // 2. 清空并重建缓冲区FreeList
        m_bufferFreeList.clear();
        constexpr uint32_t bufferCapacity = BUFFER_INDEX_END - BUFFER_INDEX_START + 1;
        m_bufferFreeList.reserve(bufferCapacity);
        for (uint32_t i = 0; i < bufferCapacity; ++i)
        {
            m_bufferFreeList.push_back(i + BUFFER_INDEX_START);
        }

        // 3. 重置统计信息
        m_allocatedTextureCount = 0;
        m_allocatedBufferCount  = 0;
    }

    // ========================================================================
    // 内部辅助方法（不加锁，调用方需持有锁）
    // ========================================================================

    uint32_t BindlessIndexAllocator::AllocateTextureIndexInternal()
    {
        // FreeList为空 → 无可用索引
        if (m_textureFreeList.empty())
        {
            enigma::core::LogError("BindlessIndexAllocator",
                                   "AllocateTextureIndex: no available texture index (1M limit reached)");
            return INVALID_INDEX;
        }

        // O(1): 从栈顶取出索引
        uint32_t index = m_textureFreeList.back();
        m_textureFreeList.pop_back();

        m_allocatedTextureCount++;

        enigma::core::LogInfo("BindlessIndexAllocator",
                              "AllocateTextureIndex: allocated index %u, total: %u",
                              index, m_allocatedTextureCount);

        return index;
    }

    uint32_t BindlessIndexAllocator::AllocateBufferIndexInternal()
    {
        // FreeList为空 → 无可用索引
        if (m_bufferFreeList.empty())
        {
            enigma::core::LogError("BindlessIndexAllocator",
                                   "AllocateBufferIndex: no available buffer index (1M limit reached)");
            return INVALID_INDEX;
        }

        // O(1): 从栈顶取出索引
        uint32_t index = m_bufferFreeList.back();
        m_bufferFreeList.pop_back();

        m_allocatedBufferCount++;

        enigma::core::LogInfo("BindlessIndexAllocator",
                              "AllocateBufferIndex: allocated index %u, total: %u",
                              index, m_allocatedBufferCount);

        return index;
    }

    bool BindlessIndexAllocator::FreeTextureIndexInternal(uint32_t index)
    {
        // 1. 验证索引范围
        if (index < TEXTURE_INDEX_START || index > TEXTURE_INDEX_END)
        {
            enigma::core::LogError("BindlessIndexAllocator",
                                   "FreeTextureIndex: invalid index %u (valid range: %u-%u)",
                                   index, TEXTURE_INDEX_START, TEXTURE_INDEX_END);
            return false;
        }

        // 2. 检查是否已释放（double free检测）
        // 注意: FreeList架构下无法直接检测double free
        // 可选: 维护一个std::unordered_set<uint32_t>来跟踪已释放索引
        // 这里为了性能优先，不做double free检测

        // 3. O(1): 将索引推入栈
        m_textureFreeList.push_back(index);
        m_allocatedTextureCount--;

        enigma::core::LogInfo("BindlessIndexAllocator",
                              "FreeTextureIndex: freed index %u, remaining: %u",
                              index, m_allocatedTextureCount);

        return true;
    }

    bool BindlessIndexAllocator::FreeBufferIndexInternal(uint32_t index)
    {
        // 1. 验证索引范围
        if (index < BUFFER_INDEX_START || index > BUFFER_INDEX_END)
        {
            enigma::core::LogError("BindlessIndexAllocator",
                                   "FreeBufferIndex: invalid index %u (valid range: %u-%u)",
                                   index, BUFFER_INDEX_START, BUFFER_INDEX_END);
            return false;
        }

        // 2. O(1): 将索引推入栈
        m_bufferFreeList.push_back(index);
        m_allocatedBufferCount--;

        enigma::core::LogInfo("BindlessIndexAllocator",
                              "FreeBufferIndex: freed index %u, remaining: %u",
                              index, m_allocatedBufferCount);

        return true;
    }
} // namespace enigma::graphic
