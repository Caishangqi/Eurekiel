#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace enigma::graphic
{
    // 前向声明
    class D12Texture;
    class D12Buffer;

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
     * @note 这是为Enigma延迟渲染器专门设计的Bindless资源管理系统
     */
    class BindlessResourceManager final
    {
    public:
        /**
         * @brief 资源类型枚举
         * 
         * 教学要点: 不同类型的资源需要不同的描述符视图和绑定方式
         */
        enum class ResourceType
        {
            Texture2D, // 2D纹理资源 (SRV)
            Texture3D, // 3D纹理资源 (SRV)
            TextureCube, // 立方体纹理 (SRV)
            Buffer, // 结构化缓冲区 (SRV)
            ConstantBuffer, // 常量缓冲区 (CBV)
            RWTexture2D, // 可读写2D纹理 (UAV)
            RWBuffer // 可读写缓冲区 (UAV)
        };

        /**
         * @brief 描述符堆类型
         */
        enum class HeapType
        {
            CBV_SRV_UAV, // 常量缓冲区、着色器资源、无序访问视图
            Sampler // 采样器堆 (暂不使用，为未来扩展预留)
        };

    private:
        /**
         * @brief 资源句柄结构
         * 
         * 教学要点: 封装资源的元数据，便于管理和调试
         */
        struct ResourceHandle
        {
            uint32_t                    index; // 全局唯一索引
            ResourceType                type; // 资源类型
            ID3D12Resource*             resource; // DX12资源指针
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle; // CPU描述符句柄
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle; // GPU描述符句柄 (着色器可见)
            std::string                 debugName; // 调试名称
            bool                        isValid; // 有效性标记

            ResourceHandle();

            // 重置句柄为无效状态
            void Reset();
        };

        /**
         * @brief 描述符堆包装器
         */
        struct DescriptorHeap
        {
            ID3D12DescriptorHeap*       heap; // DX12描述符堆
            HeapType                    type; // 堆类型
            uint32_t                    capacity; // 总容量
            uint32_t                    used; // 已使用数量
            uint32_t                    descriptorSize; // 单个描述符大小
            D3D12_CPU_DESCRIPTOR_HANDLE cpuStart; // CPU起始句柄
            D3D12_GPU_DESCRIPTOR_HANDLE gpuStart; // GPU起始句柄

            DescriptorHeap();
            ~DescriptorHeap();

            // 获取指定索引的CPU句柄
            D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;

            // 获取指定索引的GPU句柄
            D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

            // 检查是否还有空间
            bool HasSpace() const { return used < capacity; }

            // 禁用拷贝
            DescriptorHeap(const DescriptorHeap&)            = delete;
            DescriptorHeap& operator=(const DescriptorHeap&) = delete;
        };

        // 核心资源 - 设备从D3D12RenderSystem::GetDevice()获取，不在此重复存储

        // 描述符堆管理
        std::unique_ptr<DescriptorHeap> m_mainHeap; // 主描述符堆 (CBV/SRV/UAV)
        std::unique_ptr<DescriptorHeap> m_samplerHeap; // 采样器堆 (未来使用)

        // 资源管理
        std::vector<std::unique_ptr<ResourceHandle>>  m_resources; // 所有已注册资源
        std::queue<uint32_t>                          m_freeIndices; // 空闲索引队列
        std::unordered_map<ID3D12Resource*, uint32_t> m_resourceToIndex; // 资源到索引映射

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
        // 资源注册接口
        // ========================================================================

        /**
         * @brief 注册2D纹理资源
         * @param texture 纹理对象指针
         * @param debugName 调试名称
         * @return 全局资源索引，失败返回UINT32_MAX
         * 
         * 教学要点:
         * 1. 创建Shader Resource View (SRV)
         * 2. 分配全局唯一索引
         * 3. 在着色器中可以通过索引直接访问: Texture2D textures[] : register(t0, space0);
         */
        uint32_t RegisterTexture2D(const std::shared_ptr<D12Texture>& texture, const std::string& debugName = "");

        /**
         * @brief 注册结构化缓冲区
         * @param buffer 缓冲区对象指针
         * @param debugName 调试名称
         * @return 全局资源索引，失败返回UINT32_MAX
         * 
         * 教学要点: 结构化缓冲区用于存储复杂数据结构，如顶点、光源等
         */
        uint32_t RegisterStructuredBuffer(const std::shared_ptr<D12Buffer>& buffer, const std::string& debugName = "");

        /**
         * @brief 注册常量缓冲区
         * @param buffer 常量缓冲区指针
         * @param debugName 调试名称
         * @return 全局资源索引，失败返回UINT32_MAX
         * 
         * 教学要点: 常量缓冲区存储Uniform数据，如变换矩阵、光照参数等
         */
        uint32_t RegisterConstantBuffer(const std::shared_ptr<D12Buffer>& buffer, const std::string& debugName = "");

        /**
         * @brief 注册可读写纹理 (用于Compute Shader)
         * @param texture 纹理对象指针
         * @param debugName 调试名称
         * @return 全局资源索引，失败返回UINT32_MAX
         * 
         * 教学要点: UAV允许Compute Shader读写纹理，用于后处理效果
         */
        uint32_t RegisterRWTexture2D(const std::shared_ptr<D12Texture>& texture, const std::string& debugName = "");

        /**
         * @brief 注册可读写缓冲区
         * @param buffer 缓冲区对象指针
         * @param debugName 调试名称
         * @return 全局资源索引，失败返回UINT32_MAX
         */
        uint32_t RegisterRWBuffer(const std::shared_ptr<D12Buffer>& buffer, const std::string& debugName = "");

        // ========================================================================
        // 资源管理接口
        // ========================================================================

        /**
         * @brief 注销资源 (释放索引供复用)
         * @param index 要注销的资源索引
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: 
         * 1. 将索引加入空闲队列供复用
         * 2. 清理描述符视图
         * 3. 移除资源映射关系
         */
        bool UnregisterResource(uint32_t index);

        /**
         * @brief 更新已注册资源的内容
         * @param index 资源索引
         * @param newResource 新的资源对象
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: 支持动态更新资源，无需重新分配索引
         */
        bool UpdateResource(uint32_t index, const std::shared_ptr<D12Texture>& newResource);
        bool UpdateResource(uint32_t index, const std::shared_ptr<D12Buffer>& newResource);

        /**
         * @brief 批量注册资源 (提升性能)
         * @param textures 纹理数组
         * @param startIndex 起始索引输出
         * @return 成功注册的数量
         * 
         * 教学要点: 批量操作减少锁竞争，提升多线程性能
         */
        uint32_t BatchRegisterTextures(const std::vector<std::shared_ptr<D12Texture>>& textures, uint32_t& startIndex);

        // ========================================================================
        // 描述符堆访问接口
        // ========================================================================

        /**
         * @brief 获取主描述符堆 (用于根签名绑定)
         * @return 描述符堆指针
         * 
         * 教学要点: 着色器需要通过根签名绑定这个堆才能访问资源
         */
        ID3D12DescriptorHeap* GetMainDescriptorHeap() const;

        /**
         * @brief 获取采样器描述符堆
         * @return 采样器堆指针 (当前未使用)
         */
        ID3D12DescriptorHeap* GetSamplerDescriptorHeap() const;

        /**
         * @brief 获取资源的GPU描述符句柄
         * @param index 资源索引
         * @return GPU描述符句柄
         * 
         * 教学要点: GPU句柄用于着色器内的资源访问
         */
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

        /**
         * @brief 设置到命令列表 (绑定描述符堆)
         * @param commandList 命令列表指针
         * 
         * 教学要点: 每个命令列表录制前需要绑定描述符堆
         */
        void SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList) const;

        // ========================================================================
        // 查询和统计接口
        // ========================================================================

        /**
         * @brief 检查资源索引是否有效
         * @param index 要检查的索引
         * @return 有效返回true，否则返回false
         */
        bool IsValidIndex(uint32_t index) const;

        /**
         * @brief 获取资源类型
         * @param index 资源索引
         * @return 资源类型，无效索引返回最大值
         */
        ResourceType GetResourceType(uint32_t index) const;

        /**
         * @brief 获取资源调试名称
         * @param index 资源索引
         * @return 调试名称字符串
         */
        std::string GetResourceDebugName(uint32_t index) const;

        /**
         * @brief 获取当前使用的资源数量
         * @return 已使用的资源数量
         */
        uint32_t GetUsedResourceCount() const { return m_currentUsed; }

        /**
         * @brief 获取峰值使用资源数量
         * @return 峰值使用数量
         */
        uint32_t GetPeakResourceCount() const { return m_peakUsed; }

        /**
         * @brief 获取总分配数量 (包括已释放的)
         * @return 总分配数量
         */
        uint32_t GetTotalAllocatedCount() const { return m_totalAllocated; }

        /**
         * @brief 获取当前描述符堆容量
         * @return 堆容量
         */
        uint32_t GetHeapCapacity() const;

        /**
         * @brief 获取描述符堆使用率
         * @return 使用率 (0.0 - 1.0)
         */
        float GetHeapUsageRatio() const;

        /**
         * @brief 检查管理器是否已初始化
         * @return 已初始化返回true，否则返回false
         */
        bool IsInitialized() const { return m_initialized; }

    private:
        // ========================================================================
        // 私有辅助方法
        // ========================================================================

        /**
         * @brief 创建描述符堆
         * @param type 堆类型
         * @param capacity 容量
         * @return 创建的堆对象
         */
        std::unique_ptr<DescriptorHeap> CreateDescriptorHeap(HeapType type, uint32_t capacity);

        /**
         * @brief 扩容描述符堆
         * @param newCapacity 新容量
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: 扩容需要重新创建堆并复制现有描述符
         */
        bool GrowDescriptorHeap(uint32_t newCapacity);

        /**
         * @brief 分配新的资源索引
         * @return 分配的索引，失败返回UINT32_MAX
         */
        uint32_t AllocateIndex();

        /**
         * @brief 释放资源索引
         * @param index 要释放的索引
         */
        void FreeIndex(uint32_t index);

        /**
         * @brief 创建资源的描述符视图
         * @param handle 资源句柄
         * @param resource DX12资源指针
         * @param type 资源类型
         */
        void CreateDescriptorView(ResourceHandle* handle, ID3D12Resource* resource, ResourceType type);

        /**
         * @brief 获取资源类型的字符串名称 (调试用)
         * @param type 资源类型
         * @return 类型名称
         */
        static const char* GetResourceTypeName(ResourceType type);

        /**
         * @brief 查询硬件支持的最大描述符数量
         * @return 最大支持数量
         */
        uint32_t QueryMaxDescriptorCount() const;

        // 禁用拷贝构造和赋值操作
        BindlessResourceManager(const BindlessResourceManager&)            = delete;
        BindlessResourceManager& operator=(const BindlessResourceManager&) = delete;

        // 支持移动语义
        BindlessResourceManager(BindlessResourceManager&&)            = default;
        BindlessResourceManager& operator=(BindlessResourceManager&&) = default;
    };
} // namespace enigma::graphic
