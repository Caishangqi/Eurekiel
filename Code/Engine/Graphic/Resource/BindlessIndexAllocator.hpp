#pragma once

#include <vector>
#include <mutex>
#include <cstdint>

namespace enigma::graphic
{
    /**
     * @brief BindlessIndexAllocator - SM6.6纯索引分配器
     *
     * 教学要点:
     * 1. 单一职责: 只负责索引的分配和释放
     * 2. 索引范围:
     *    - 纹理: 0 - 999,999
     *    - 缓冲区: 1,000,000 - 1,999,999
     * 3. 线程安全: 使用std::mutex保护
     * 4. 零资源管理: 不存储资源指针,不调用资源方法
     *
     * SM6.6 Bindless架构(Milestone 2.7):
     * - 与传统方式对比: 移除所有描述符堆、GPU句柄、Descriptor Table逻辑
     * - 职责分离: 索引分配(此类) vs 描述符创建(GlobalDescriptorHeapManager)
     * - 性能优化: 简单位图查找,无复杂资源映射表
     * - 代码简化: 70%代码量减少,从~800行减至~200行
     *
     * 使用示例:
     * ```cpp
     * // 1. 分配索引
     * auto allocator = D3D12RenderSystem::GetBindlessIndexAllocator();
     * uint32_t textureIndex = allocator->AllocateTextureIndex();
     *
     * // 2. 资源存储索引
     * texture->SetBindlessIndex(textureIndex);
     *
     * // 3. 在全局堆创建描述符
     * texture->CreateDescriptorInGlobalHeap(device, heapManager);
     *
     * // 4. HLSL直接访问
     * // Texture2D tex = ResourceDescriptorHeap[textureIndex];
     * ```
     */
    class BindlessIndexAllocator final
    {
    public:
        // 索引范围常量
        static constexpr uint32_t TEXTURE_INDEX_START  = 0;
        static constexpr uint32_t TEXTURE_INDEX_END    = 999'999;
        static constexpr uint32_t BUFFER_INDEX_START   = 1'000'000;
        static constexpr uint32_t BUFFER_INDEX_END     = 1'999'999;
        static constexpr uint32_t INVALID_INDEX        = UINT32_MAX;

        BindlessIndexAllocator();
        ~BindlessIndexAllocator() = default;

        // 禁用拷贝和移动
        BindlessIndexAllocator(const BindlessIndexAllocator&) = delete;
        BindlessIndexAllocator& operator=(const BindlessIndexAllocator&) = delete;
        BindlessIndexAllocator(BindlessIndexAllocator&&) = delete;
        BindlessIndexAllocator& operator=(BindlessIndexAllocator&&) = delete;

        // ====================================================================
        // 核心索引分配接口
        // ====================================================================

        /**
         * @brief 分配纹理索引
         * @return 成功返回索引(0-999999),失败返回INVALID_INDEX
         *
         * 教学要点:
         * 1. 线性搜索空闲索引 (可优化为位扫描算法)
         * 2. 线程安全: std::lock_guard自动加锁
         * 3. 容量检查: 达到1M上限时返回失败
         */
        uint32_t AllocateTextureIndex();

        /**
         * @brief 分配缓冲区索引
         * @return 成功返回索引(1000000-1999999),失败返回INVALID_INDEX
         */
        uint32_t AllocateBufferIndex();

        /**
         * @brief 释放纹理索引
         * @param index 要释放的索引(0-999999)
         * @return 成功返回true
         *
         * 教学要点:
         * 1. 索引范围验证
         * 2. 重复释放检测
         * 3. 统计信息更新
         */
        bool FreeTextureIndex(uint32_t index);

        /**
         * @brief 释放缓冲区索引
         * @param index 要释放的索引(1000000-1999999)
         * @return 成功返回true
         */
        bool FreeBufferIndex(uint32_t index);

        // ====================================================================
        // 统计查询接口
        // ====================================================================

        /**
         * @brief 获取已分配纹理数量
         */
        uint32_t GetAllocatedTextureCount() const;

        /**
         * @brief 获取已分配缓冲区数量
         */
        uint32_t GetAllocatedBufferCount() const;

        /**
         * @brief 获取可用纹理数量
         */
        uint32_t GetAvailableTextureCount() const;

        /**
         * @brief 获取可用缓冲区数量
         */
        uint32_t GetAvailableBufferCount() const;

        /**
         * @brief 重置分配器 (释放所有索引)
         *
         * 教学要点: 用于场景切换或资源重载
         */
        void Reset();

    private:
        // 纹理索引位图 (1M位 = 125KB)
        std::vector<bool> m_textureIndexUsed;

        // 缓冲区索引位图 (1M位 = 125KB)
        std::vector<bool> m_bufferIndexUsed;

        // 统计信息
        uint32_t m_allocatedTextureCount;
        uint32_t m_allocatedBufferCount;

        // 线程安全
        mutable std::mutex m_mutex;

        // 辅助方法
        uint32_t FindFreeTextureIndex();
        uint32_t FindFreeBufferIndex();
    };

} // namespace enigma::graphic