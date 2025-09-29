#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <string>
#include <optional>
#include <d3d12.h>
#include <dxgi1_6.h>

// 新的描述符管理类
#include "DescriptorHeapManager.hpp"
#include "DescriptorHandle.hpp"
#include "BindlessResourceTypes.hpp"

namespace enigma::graphic
{
    // 前向声明
    class D12Resource;
    class D12Texture;
    class D12Buffer;

    /**
     * @brief 描述符堆类型
     *
     * 重构说明: 此枚举已不再使用，现在使用DescriptorHeapManager::HeapType
     * 保留用于向后兼容，未来版本可能删除
     */
    enum class HeapType
    {
        CBV_SRV_UAV, // 常量缓冲区、着色器资源、无序访问视图
        Sampler // 采样器堆 (暂不使用，为未来扩展预留)
    };

    /**
     * @brief BindlessResourceManager类 - DirectX 12 Bindless资源绑定管理器
     *
     * 教学要点:
     * 1. Bindless渲染是现代GPU的重要特性，突破传统描述符表的限制
     * 2. 传统方式：每次Draw Call前需要绑定描述符表，限制同时可见的资源数量
     * 3. Bindless方式：创建巨大的描述符堆，着色器通过索引直接访问资源
     * 4. 性能优势：减少状态切换，支持GPU-Driven渲染，提升批处理效率
     *
     * DirectX 12特性:
     * - 支持百万级别的描述符 (受硬件Tier限制)
     * - GPU可见的描述符堆 (DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
     * - 动态描述符索引 (Dynamic Indexing)
     * - 资源状态自动管理
     *
     * 延迟渲染应用:
     * - G-Buffer阶段：大量材质纹理的高效访问
     * - 光照计算：动态光源和阴影贴图的批量绑定
     * - 后处理：临时RT的快速切换，无需重新绑定描述符表
     * - Compute Shader：SSAO、Bloom等后处理的资源访问
     *
     * 🔥 重构变更说明 (Milestone 2):
     * 1. **分离描述符管理职责**: 原内嵌DescriptorHeap结构体已抽取为独立的DescriptorHeapManager类
     * 2. **RAII资源管理**: 引入DescriptorHandle类实现自动描述符生命周期管理
     * 3. **遵循单一职责原则**: BindlessResourceManager专注于资源索引分配和视图创建
     * 4. **简化架构复杂性**: 描述符堆的创建、分配、回收全部委托给专用管理器
     * 5. **现代C++实践**: 智能指针、RAII、移动语义的深度应用
     *
     * @note 这是为Enigma延迟渲染器专门设计的Bindless资源管理系统，已重构为使用独立的描述符管理架构
     */
    class BindlessResourceManager final
    {
    private:
        // 核心资源 - 设备从D3D12RenderSystem::GetDevice()获取，不在此重复存储

        // 描述符堆管理 - 重构为使用新的独立管理类
        std::shared_ptr<DescriptorHeapManager> m_heapManager; // 描述符堆管理器

        // 🔥 简化的资源管理 (移除ResourceHandle)
        std::vector<std::shared_ptr<D12Resource>>          m_registeredResources; // 已注册资源数组 (按索引访问)
        std::queue<uint32_t>                               m_freeIndices; // 空闲索引队列
        std::unordered_map<ID3D12Resource*, uint32_t>      m_resourceToIndex; // 资源到索引映射
        std::unordered_map<uint32_t, BindlessResourceType> m_indexToType; // 索引到资源类型映射 (用于调试)

        // 动态扩容参数
        uint32_t m_initialCapacity; // 初始容量
        uint32_t m_growthFactor; // 扩容因子
        uint32_t m_maxCapacity; // 最大容量

        // 统计信息
        uint32_t m_totalAllocated; // 总分配数量
        uint32_t m_currentUsed; // 当前使用数量
        uint32_t m_peakUsed; // 峰值使用数量

        // 线程安全
        std::mutex m_mutex; // 互斥锁
        bool       m_initialized; // 初始化标记

        // 默认配置参数
        static constexpr uint32_t DEFAULT_INITIAL_CAPACITY = 10000; // 默认初始容量
        static constexpr uint32_t DEFAULT_GROWTH_FACTOR    = 2; // 默认扩容因子
        static constexpr uint32_t DEFAULT_MAX_CAPACITY     = 1000000; // 默认最大容量

    public:
        /**
         * @brief 构造函数
         * 
         * 教学要点: 构造函数只做基本初始化，避免复杂操作
         */
        BindlessResourceManager();

        /**
         * @brief 析构函数
         * 
         * 教学要点: 确保所有资源正确释放
         */
        ~BindlessResourceManager();

        // ========================================================================
        // 生命周期管理
        // ========================================================================

        /**
         * @brief 初始化Bindless资源管理器
         * @param initialCapacity 初始描述符容量
         * @param maxCapacity 最大描述符容量
         * @param growthFactor 扩容因子 (新容量 = 旧容量 * 因子)
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. 从D3D12RenderSystem::GetDevice()获取设备，避免重复存储
         * 2. 创建GPU可见的描述符堆 (SHADER_VISIBLE标志)
         * 3. 查询硬件支持的最大描述符数量
         * 4. 预分配资源句柄数组，避免频繁重分配
         * 5. 初始化索引分配器
         *
         * 重构实现指导:
         * ```cpp
         * // 1. 创建并初始化DescriptorHeapManager
         * m_descriptorHeapManager = std::make_shared<DescriptorHeapManager>();
         * if (!m_descriptorHeapManager->Initialize(initialCapacity, 2048)) { // CBV/SRV/UAV容量, Sampler容量
         *     return false;
         * }
         *
         * // 2. 预分配资源数组 (简化为共享指针数组)
         * m_registeredResources.resize(initialCapacity);
         *
         * // 3. 初始化空闲索引队列
         * for (uint32_t i = 0; i < initialCapacity; ++i) {
         *     m_freeIndices.push(i);
         * }
         *
         * // 4. 设置参数和标记
         * m_initialCapacity = initialCapacity;
         * m_maxCapacity = maxCapacity;
         * m_growthFactor = growthFactor;
         * m_initialized = true;
         * ```
         */
        bool Initialize(uint32_t initialCapacity = DEFAULT_INITIAL_CAPACITY,
                        uint32_t maxCapacity     = DEFAULT_MAX_CAPACITY,
                        uint32_t growthFactor    = DEFAULT_GROWTH_FACTOR);

        /**
         * @brief 释放所有资源
         * 
         * 教学要点: 按正确顺序释放资源，避免悬空指针
         */
        void Shutdown();

        // ========================================================================
        // 资源注册接口 (🔥 新简化架构 - 移除ResourceHandle)
        // ========================================================================

        /**
         * @brief 注册纹理到Bindless系统
         * @param texture 纹理资源
         * @param type 资源类型
         * @return 成功返回全局索引，失败返回nullopt
         *
         * 实现指导:
         * 1. 检查纹理是否已启用Bindless支持，未启用则自动启用
         * 2. 从描述符堆分配CBV/SRV/UAV描述符
         * 3. 创建纹理的SRV到分配的描述符位置
         * 4. 将纹理存储到已注册资源列表
         * 5. 更新索引映射和统计信息
         * 6. 调用texture->SetBindlessBinding设置绑定信息
         */
        std::optional<uint32_t> RegisterTexture2D(std::shared_ptr<D12Texture> texture, BindlessResourceType type = BindlessResourceType::Texture2D);

        /**
         * @brief 注册缓冲区到Bindless系统
         * @param buffer 缓冲区资源
         * @param type 资源类型
         * @return 成功返回全局索引，失败返回nullopt
         *
         * 实现指导:
         * 1. 检查缓冲区是否已启用Bindless支持，未启用则自动启用
         * 2. 根据类型创建对应的视图 (CBV for ConstantBuffer, SRV for StructuredBuffer)
         * 3. 从描述符堆分配描述符
         * 4. 存储到已注册资源列表并更新映射
         * 5. 调用buffer->SetBindlessBinding设置绑定信息
         */
        std::optional<uint32_t> RegisterBuffer(std::shared_ptr<D12Buffer> buffer, BindlessResourceType type);

        /**
         * @brief 从Bindless系统注销资源
         * @param resource 要注销的资源
         * @return 成功返回true，失败返回false
         *
         * 实现指导:
         * 1. 检查资源是否已注册到Bindless系统
         * 2. 获取资源的Bindless索引
         * 3. 从已注册资源列表中获取资源的DescriptorHandle
         * 4. 通过DescriptorHandle释放描述符堆中的描述符
         * 5. 从已注册资源列表和索引映射中移除
         * 6. 调用resource->ClearBindlessBinding清除绑定信息
         * 7. 更新统计信息
         */
        bool UnregisterResource(std::shared_ptr<D12Resource> resource);

        /**
         * @brief 获取资源的GPU描述符句柄
         * @param resource 资源对象
         * @return 如果资源已注册返回GPU句柄，否则返回nullopt
         *
         * 实现指导:
         * 1. 检查资源是否已注册到Bindless系统
         * 2. 从资源的ResourceBindingTraits获取DescriptorHandle
         * 3. 通过DescriptorHandle获取GPU描述符句柄
         * 4. 返回GPU描述符句柄用于着色器绑定表设置
         */
        std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> GetGPUHandle(std::shared_ptr<D12Resource> resource) const;

        /**
         * @brief 根据全局索引获取GPU描述符句柄
         * @param globalIndex 全局索引
         * @return 如果索引有效返回GPU句柄，否则返回nullopt
         *
         * 实现指导:
         * 1. 验证索引范围 (0 <= globalIndex < m_registeredResources.size())
         * 2. 从已注册资源列表获取资源
         * 3. 检查资源是否仍然有效
         * 4. 获取资源的GPU描述符句柄并返回
         */
        std::optional<D3D12_GPU_DESCRIPTOR_HANDLE> GetGPUHandleByIndex(uint32_t globalIndex) const;

        /**
         * @brief 批量更新描述符表到着色器绑定
         * @param commandList 命令列表
         * @param rootParameterIndex 根参数索引
         * @param startIndex 起始索引
         * @param count 描述符数量
         *
         * 实现指导:
         * 1. 验证索引范围和命令列表有效性
         * 2. 获取起始GPU描述符句柄
         * 3. 调用ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable
         * 4. 支持计算着色器的SetComputeRootDescriptorTable
         */
        void SetDescriptorTable(ID3D12GraphicsCommandList* commandList, uint32_t rootParameterIndex, uint32_t startIndex, uint32_t count);

        /**
         * @brief 刷新所有已注册资源的描述符
         *
         * 实现指导:
         * 1. 遍历所有已注册资源
         * 2. 获取每个资源的DescriptorHandle
         * 3. 重新创建描述符视图到相同的描述符位置
         * 4. 更新GPU可见堆中的描述符
         *
         * 使用场景:
         * - 纹理内容更新后需要刷新SRV
         * - 缓冲区大小改变后需要刷新CBV/SRV
         */
        void RefreshAllDescriptors();

        // ========================================================================
        // 描述符堆访问接口 (🔥 更新为DescriptorHeapManager架构)
        // ========================================================================

        /**
         * @brief 获取主描述符堆 (用于根签名绑定)
         * @return 描述符堆指针
         *
         * 实现指导:
         * ```cpp
         * return m_descriptorHeapManager ? m_descriptorHeapManager->GetCbvSrvUavHeap() : nullptr;
         * ```
         */
        ID3D12DescriptorHeap* GetMainDescriptorHeap() const;

        /**
         * @brief 获取采样器描述符堆
         * @return 采样器堆指针 (当前未使用)
         *
         * 实现指导:
         * ```cpp
         * return m_descriptorHeapManager ? m_descriptorHeapManager->GetSamplerHeap() : nullptr;
         * ```
         */
        ID3D12DescriptorHeap* GetSamplerDescriptorHeap() const;

        /**
         * @brief 设置到命令列表 (绑定描述符堆)
         * @param commandList 命令列表指针
         *
         * 实现指导:
         * ```cpp
         * if (m_descriptorHeapManager && commandList) {
         *     m_descriptorHeapManager->SetDescriptorHeaps(commandList);
         * }
         * ```
         */
        void SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList) const;

        // ========================================================================
        // 查询和统计接口 (🔥 简化架构)
        // ========================================================================

        /**
         * @brief 检查是否已初始化
         */
        bool IsInitialized() const { return m_initialized; }

        /**
         * @brief 获取已注册资源数量
         */
        uint32_t GetRegisteredResourceCount() const { return static_cast<uint32_t>(m_registeredResources.size()); }

        /**
         * @brief 获取指定类型的已注册资源数量
         * @param type 资源类型
         *
         * 实现指导:
         * 1. 遍历m_indexToType映射
         * 2. 统计指定类型的资源数量
         */
        uint32_t GetResourceCountByType(BindlessResourceType type) const;

        /**
         * @brief 获取描述符堆使用情况
         * @return {已使用数量, 总容量}
         *
         * 实现指导:
         * 委托给m_descriptorHeapManager->GetDescriptorHeapUsage()
         */
        std::pair<uint32_t, uint32_t> GetDescriptorHeapUsage() const;

        /**
         * @brief 获取调试信息字符串
         *
         * 实现指导:
         * 1. 包含已注册资源总数
         * 2. 按类型统计资源数量
         * 3. 描述符堆使用情况
         * 4. 每个资源的详细信息 (名称、索引、类型)
         */
        std::string GetDebugInfo() const;

        /**
         * @brief 获取描述符堆管理器
         * @return 描述符堆管理器指针
         * @details
         * 提供对底层描述符堆管理器的访问，主要用于：
         * - SwapChain RTV描述符分配
         * - 外部系统需要直接分配描述符的场景
         * - 调试和性能分析
         */
        DescriptorHeapManager* GetDescriptorHeapManager() const
        {
            return m_heapManager.get();
        }

    private:
        // ========================================================================
        // 私有辅助方法 (🔥 简化架构 - 移除ResourceHandle相关方法)
        // ========================================================================

        /**
         * @brief 分配新的资源索引
         * @return 分配的索引，失败返回nullopt
         *
         * 实现指导:
         * ```cpp
         * std::lock_guard<std::mutex> lock(m_mutex);
         * if (m_freeIndices.empty()) {
         *     // 需要扩容资源数组
         *     uint32_t newSize = std::min(m_registeredResources.size() * m_growthFactor, m_maxCapacity);
         *     if (newSize <= m_registeredResources.size()) return std::nullopt;
         *
         *     // 扩容资源数组并添加新索引到空闲队列
         *     uint32_t oldSize = m_registeredResources.size();
         *     m_registeredResources.resize(newSize);
         *     for (uint32_t i = oldSize; i < newSize; ++i) {
         *         m_freeIndices.push(i);
         *     }
         * }
         *
         * uint32_t index = m_freeIndices.front();
         * m_freeIndices.pop();
         * m_currentUsed++;
         * m_peakUsed = std::max(m_peakUsed, m_currentUsed);
         * return index;
         * ```
         */
        std::optional<uint32_t> AllocateIndex();

        /**
         * @brief 释放资源索引
         * @param index 要释放的索引
         *
         * 实现指导:
         * ```cpp
         * std::lock_guard<std::mutex> lock(m_mutex);
         * if (index < m_registeredResources.size() && m_registeredResources[index]) {
         *     // 从资源映射中移除
         *     auto resource = m_registeredResources[index];
         *     if (resource && resource->GetResource()) {
         *         m_resourceToIndex.erase(resource->GetResource());
         *     }
         *     m_indexToType.erase(index);
         *
         *     // 清空资源槽位并回收索引
         *     m_registeredResources[index].reset();
         *     m_freeIndices.push(index);
         *     m_currentUsed--;
         * }
         * ```
         */
        void FreeIndex(uint32_t index);

        /**
         * @brief 创建资源的描述符视图
         * @param resource D12资源对象
         * @param descriptorHandle 目标描述符句柄
         * @param type 资源类型
         *
         * 实现指导:
         * 1. 根据资源类型创建对应的描述符视图 (SRV, CBV, UAV)
         * 2. 使用descriptorHandle.GetCpuHandle()作为目标位置
         * 3. 调用D3D12RenderSystem::GetDevice()获取设备
         * 4. 使用设备的Create*View方法创建描述符
         */
        void CreateDescriptorView(std::shared_ptr<D12Resource> resource,
                                  const DescriptorHandle&      descriptorHandle,
                                  BindlessResourceType         type);

        /**
         * @brief 获取资源类型的字符串名称 (调试用)
         * @param type 资源类型
         * @return 类型名称
         */
        static const char* GetResourceTypeName(BindlessResourceType type);

        /**
         * @brief 验证资源索引是否有效
         * @param index 要验证的索引
         * @return 有效返回true，否则返回false
         *
         * 实现指导:
         * ```cpp
         * return index < m_registeredResources.size() && m_registeredResources[index] != nullptr;
         * ```
         */
        bool IsValidIndex(uint32_t index) const;

        // 禁用拷贝构造和赋值操作
        BindlessResourceManager(const BindlessResourceManager&)            = delete;
        BindlessResourceManager& operator=(const BindlessResourceManager&) = delete;

        // 支持移动语义
        BindlessResourceManager(BindlessResourceManager&&)            = default;
        BindlessResourceManager& operator=(BindlessResourceManager&&) = default;
    };
} // namespace enigma::graphic
