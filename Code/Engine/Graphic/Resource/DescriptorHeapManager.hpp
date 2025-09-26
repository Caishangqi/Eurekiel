#pragma once

#include <memory>
#include <vector>
#include <mutex>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

namespace enigma::graphic
{
    /**
     * @brief DescriptorHeapManager类 - DirectX 12描述符堆管理器
     *
     * 教学要点:
     * 1. 描述符堆是DirectX 12资源访问的核心概念
     * 2. GPU可见堆允许着色器直接通过索引访问资源
     * 3. 描述符生命周期管理是现代图形API的关键挑战
     * 4. 单一职责原则：专注于描述符堆的创建、分配、回收
     *
     * 设计原则:
     * - 单一职责 (SRP): 只负责描述符堆管理
     * - 开放封闭 (OCP): 通过DescriptorHandle接口扩展
     * - 依赖倒置 (DIP): 依赖D3D12RenderSystem获取设备
     *
     * @note 从BindlessResourceManager中抽离，提高架构清晰度
     */
    class DescriptorHeapManager final
    {
    public:
        /**
         * @brief 描述符堆类型
         */
        enum class HeapType
        {
            CBV_SRV_UAV, // 常量缓冲区、着色器资源、无序访问视图堆 (Bindless统一大堆)
            RTV, // 渲染目标视图堆 (渲染输出专用)
            DSV, // 深度模板视图堆 (深度输出专用)
            Sampler // 采样器堆
        };

        /**
         * @brief 描述符分配信息
         */
        struct DescriptorAllocation
        {
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle; // CPU描述符句柄
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle; // GPU描述符句柄 (着色器可见)
            uint32_t                    heapIndex; // 堆内索引
            HeapType                    heapType; // 堆类型
            bool                        isValid; // 有效性标记

            DescriptorAllocation();
            void Reset();
        };

    private:
        /**
         * @brief 描述符堆包装器 - 内部实现
         */
        struct DescriptorHeap
        {
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap; // DX12描述符堆
            HeapType                                     type; // 堆类型
            uint32_t                                     capacity; // 总容量
            uint32_t                                     used; // 已使用数量
            uint32_t                                     descriptorSize; // 单个描述符大小
            D3D12_CPU_DESCRIPTOR_HANDLE                  cpuStart; // CPU起始句柄
            D3D12_GPU_DESCRIPTOR_HANDLE                  gpuStart; // GPU起始句柄

            DescriptorHeap();

            /**
             * @brief 获取指定索引的CPU句柄
             *
             * 实现指导:
             * ```cpp
             * D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuStart;
             * handle.ptr += index * descriptorSize;
             * return handle;
             * ```
             */
            D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint32_t index) const;

            /**
             * @brief 获取指定索引的GPU句柄
             *
             * 实现指导:
             * ```cpp
             * D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuStart;
             * handle.ptr += index * descriptorSize;
             * return handle;
             * ```
             */
            D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32_t index) const;

            // 检查是否还有空间
            bool HasSpace() const { return used < capacity; }

            // 禁用拷贝，支持移动
            DescriptorHeap(const DescriptorHeap&)            = delete;
            DescriptorHeap& operator=(const DescriptorHeap&) = delete;
            DescriptorHeap(DescriptorHeap&&)                 = default;
            DescriptorHeap& operator=(DescriptorHeap&&)      = default;
        };

        // 核心描述符堆 - 支持4种堆类型
        std::unique_ptr<DescriptorHeap> m_cbvSrvUavHeap; // CBV/SRV/UAV堆 (Bindless统一大堆)
        std::unique_ptr<DescriptorHeap> m_rtvHeap; // RTV堆 (渲染目标专用)
        std::unique_ptr<DescriptorHeap> m_dsvHeap; // DSV堆 (深度模板专用)
        std::unique_ptr<DescriptorHeap> m_samplerHeap; // Sampler堆

        // 索引管理 - 使用简单的位图方式
        std::vector<bool> m_cbvSrvUavUsed; // CBV/SRV/UAV堆使用状态
        std::vector<bool> m_rtvUsed; // RTV堆使用状态
        std::vector<bool> m_dsvUsed; // DSV堆使用状态
        std::vector<bool> m_samplerUsed; // Sampler堆使用状态

        // 分配策略参数
        uint32_t m_cbvSrvUavCapacity; // CBV/SRV/UAV堆容量
        uint32_t m_rtvCapacity; // RTV堆容量
        uint32_t m_dsvCapacity; // DSV堆容量
        uint32_t m_samplerCapacity; // Sampler堆容量
        uint32_t m_nextFreeCbvSrvUav; // 下一个空闲CBV/SRV/UAV索引 (优化查找)
        uint32_t m_nextFreeRtv; // 下一个空闲RTV索引 (优化查找)
        uint32_t m_nextFreeDsv; // 下一个空闲DSV索引 (优化查找)
        uint32_t m_nextFreeSampler; // 下一个空闲Sampler索引 (优化查找)

        // 统计信息
        uint32_t m_totalCbvSrvUavAllocated; // 总CBV/SRV/UAV分配数
        uint32_t m_totalRtvAllocated; // 总RTV分配数
        uint32_t m_totalDsvAllocated; // 总DSV分配数
        uint32_t m_totalSamplerAllocated; // 总Sampler分配数
        uint32_t m_peakCbvSrvUavUsed; // CBV/SRV/UAV峰值使用数
        uint32_t m_peakRtvUsed; // RTV峰值使用数
        uint32_t m_peakDsvUsed; // DSV峰值使用数
        uint32_t m_peakSamplerUsed; // Sampler峰值使用数

        // 线程安全
        mutable std::mutex m_mutex; // 保护所有操作的互斥锁
        bool               m_initialized; // 初始化状态

        // 默认配置 - 基于1A-2A-3A决策的最优配置
        static constexpr uint32_t DEFAULT_CBV_SRV_UAV_CAPACITY = 1000000; // 默认100万个CBV/SRV/UAV (Bindless统一大堆)
        static constexpr uint32_t DEFAULT_RTV_CAPACITY         = 1000; // 默认1000个RTV (渲染目标)
        static constexpr uint32_t DEFAULT_DSV_CAPACITY         = 100; // 默认100个DSV (深度模板)
        static constexpr uint32_t DEFAULT_SAMPLER_CAPACITY     = 2048; // 默认2048个Sampler

    public:
        /**
         * @brief 构造函数
         */
        DescriptorHeapManager();

        /**
         * @brief 析构函数 - RAII自动清理
         */
        ~DescriptorHeapManager();

        // ========================================================================
        // 生命周期管理
        // ========================================================================

        /**
         * @brief 初始化描述符堆管理器
         * @param cbvSrvUavCapacity CBV/SRV/UAV堆容量
         * @param samplerCapacity Sampler堆容量
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. 从D3D12RenderSystem::GetDevice()获取设备
         * 2. 创建GPU可见的描述符堆 (DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
         * 3. 预分配索引跟踪数组
         * 4. 查询硬件限制并适配
         *
         * 实现指导:
         * ```cpp
         * // 1. 获取D3D12设备
         * auto device = D3D12RenderSystem::GetDevice();
         *
         * // 2. 创建CBV/SRV/UAV堆
         * D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDesc = {};
         * cbvSrvUavDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
         * cbvSrvUavDesc.NumDescriptors = cbvSrvUavCapacity;
         * cbvSrvUavDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
         *
         * // 3. 使用CreateDescriptorHeap辅助方法
         * m_cbvSrvUavHeap = CreateDescriptorHeap(HeapType::CBV_SRV_UAV, cbvSrvUavCapacity);
         *
         * // 4. 初始化位图数组
         * m_cbvSrvUavUsed.resize(cbvSrvUavCapacity, false);
         * m_samplerUsed.resize(samplerCapacity, false);
         *
         * // 5. 设置容量和统计
         * m_cbvSrvUavCapacity = cbvSrvUavCapacity;
         * m_samplerCapacity = samplerCapacity;
         * ```
         */
        bool Initialize(uint32_t cbvSrvUavCapacity = DEFAULT_CBV_SRV_UAV_CAPACITY,
                        uint32_t rtvCapacity       = DEFAULT_RTV_CAPACITY,
                        uint32_t dsvCapacity       = DEFAULT_DSV_CAPACITY,
                        uint32_t samplerCapacity   = DEFAULT_SAMPLER_CAPACITY);

        /**
         * @brief 释放所有资源
         */
        void Shutdown();

        /**
         * @brief 检查是否已初始化
         */
        bool IsInitialized() const;

        // ========================================================================
        // 描述符分配接口
        // ========================================================================

        /**
         * @brief 分配CBV/SRV/UAV描述符
         * @return 分配的描述符信息，失败时isValid为false
         *
         * 教学要点:
         * 1. 线程安全的索引分配
         * 2. 连续索引分配策略（内存友好）
         * 3. 自动更新使用统计
         */
        DescriptorAllocation AllocateCbvSrvUav();

        /**
         * @brief 分配Sampler描述符
         * @return 分配的描述符信息，失败时isValid为false
         */
        DescriptorAllocation AllocateSampler();

        /**
         * @brief 分配RTV描述符 (渲染目标视图)
         * @return 分配的描述符信息，失败时isValid为false
         *
         * 教学要点:
         * 1. RTV专用于渲染输出，不需要SHADER_VISIBLE标志
         * 2. 用于OMSetRenderTargets()绑定，而非Bindless访问
         * 3. 支持SwapChain后台缓冲区和离屏渲染目标
         */
        DescriptorAllocation AllocateRtv();

        /**
         * @brief 分配DSV描述符 (深度模板视图)
         * @return 分配的描述符信息，失败时isValid为false
         *
         * 教学要点:
         * 1. DSV专用于深度/模板输出，不可着色器访问
         * 2. 用于OMSetRenderTargets()的深度缓冲区绑定
         * 3. 支持深度测试、模板测试和深度写入
         */
        DescriptorAllocation AllocateDsv();

        /**
         * @brief 批量分配CBV/SRV/UAV描述符
         * @param count 要分配的数量
         * @return 分配结果数组，失败的分配isValid为false
         *
         * 教学要点: 批量分配减少锁竞争，提升性能
         */
        std::vector<DescriptorAllocation> BatchAllocateCbvSrvUav(uint32_t count);

        /**
         * @brief 释放CBV/SRV/UAV描述符
         * @param allocation 要释放的描述符分配
         * @return 成功返回true，失败返回false
         *
         * 教学要点: 索引回收供后续复用，避免碎片化
         */
        bool FreeCbvSrvUav(const DescriptorAllocation& allocation);

        /**
         * @brief 释放Sampler描述符
         * @param allocation 要释放的描述符分配
         * @return 成功返回true，失败返回false
         */
        bool FreeSampler(const DescriptorAllocation& allocation);

        /**
         * @brief 释放RTV描述符
         * @param allocation 要释放的描述符分配
         * @return 成功返回true，失败返回false
         */
        bool FreeRtv(const DescriptorAllocation& allocation);

        /**
         * @brief 释放DSV描述符
         * @param allocation 要释放的描述符分配
         * @return 成功返回true，失败返回false
         */
        bool FreeDsv(const DescriptorAllocation& allocation);

        // ========================================================================
        // 描述符堆访问接口
        // ========================================================================

        /**
         * @brief 获取CBV/SRV/UAV描述符堆 (用于根签名绑定)
         * @return 描述符堆指针
         */
        ID3D12DescriptorHeap* GetCbvSrvUavHeap() const;

        /**
         * @brief 获取Sampler描述符堆
         * @return 描述符堆指针
         */
        ID3D12DescriptorHeap* GetSamplerHeap() const;

        /**
         * @brief 获取RTV描述符堆 (渲染目标视图堆)
         * @return 描述符堆指针
         *
         * 教学要点: RTV堆专用于OMSetRenderTargets()，不支持着色器访问
         */
        ID3D12DescriptorHeap* GetRtvHeap() const;

        /**
         * @brief 获取DSV描述符堆 (深度模板视图堆)
         * @return 描述符堆指针
         *
         * 教学要点: DSV堆专用于深度缓冲区绑定，不支持着色器访问
         */
        ID3D12DescriptorHeap* GetDsvHeap() const;

        /**
         * @brief 设置描述符堆到命令列表
         * @param commandList 命令列表
         *
         * 教学要点: 绘制前必须绑定描述符堆到命令列表
         */
        void SetDescriptorHeaps(ID3D12GraphicsCommandList* commandList) const;

        // ========================================================================
        // 统计和调试接口
        // ========================================================================

        /**
         * @brief 描述符堆统计信息
         */
        struct HeapStats
        {
            uint32_t cbvSrvUavCapacity; // CBV/SRV/UAV堆总容量
            uint32_t cbvSrvUavUsed; // CBV/SRV/UAV堆已使用
            uint32_t cbvSrvUavAllocated; // CBV/SRV/UAV堆总分配数
            uint32_t cbvSrvUavPeakUsed; // CBV/SRV/UAV堆峰值使用

            uint32_t rtvCapacity; // RTV堆总容量
            uint32_t rtvUsed; // RTV堆已使用
            uint32_t rtvAllocated; // RTV堆总分配数
            uint32_t rtvPeakUsed; // RTV堆峰值使用

            uint32_t dsvCapacity; // DSV堆总容量
            uint32_t dsvUsed; // DSV堆已使用
            uint32_t dsvAllocated; // DSV堆总分配数
            uint32_t dsvPeakUsed; // DSV堆峰值使用

            uint32_t samplerCapacity; // Sampler堆总容量
            uint32_t samplerUsed; // Sampler堆已使用
            uint32_t samplerAllocated; // Sampler堆总分配数
            uint32_t samplerPeakUsed; // Sampler堆峰值使用

            float cbvSrvUavUsageRatio; // CBV/SRV/UAV使用率 (0.0-1.0)
            float rtvUsageRatio; // RTV使用率 (0.0-1.0)
            float dsvUsageRatio; // DSV使用率 (0.0-1.0)
            float samplerUsageRatio; // Sampler使用率 (0.0-1.0)
        };

        /**
         * @brief 获取堆统计信息
         */
        HeapStats GetStats() const;

        /**
         * @brief 获取CBV/SRV/UAV堆使用率 (Bindless统一大堆)
         * @return 使用率 (0.0-1.0)
         */
        float GetCbvSrvUavUsageRatio() const;

        /**
         * @brief 获取RTV堆使用率 (渲染目标专用)
         * @return 使用率 (0.0-1.0)
         */
        float GetRtvUsageRatio() const;

        /**
         * @brief 获取DSV堆使用率 (深度模板专用)
         * @return 使用率 (0.0-1.0)
         */
        float GetDsvUsageRatio() const;

        /**
         * @brief 获取Sampler堆使用率
         * @return 使用率 (0.0-1.0)
         */
        float GetSamplerUsageRatio() const;

        /**
         * @brief 检查是否还有足够空间
         * @param cbvSrvUavCount 需要的CBV/SRV/UAV数量 (Bindless统一大堆)
         * @param rtvCount 需要的RTV数量 (渲染目标专用)
         * @param dsvCount 需要的DSV数量 (深度模板专用)
         * @param samplerCount 需要的Sampler数量
         * @return 所有堆都有足够空间返回true
         */
        bool HasEnoughSpace(uint32_t cbvSrvUavCount, uint32_t rtvCount = 0, uint32_t dsvCount = 0, uint32_t samplerCount = 0) const;

        /**
         * @brief 碎片整理 (当前未实现，为未来扩展预留)
         *
         * 教学要点: 描述符堆的碎片整理是复杂操作，需要重新创建堆
         */
        void DefragmentHeaps();

    private:
        // ========================================================================
        // 私有辅助方法
        // ========================================================================

        /**
         * @brief 创建描述符堆
         * @param type 堆类型
         * @param capacity 容量
         * @return 创建的堆对象
         *
         * 实现指导:
         * ```cpp
         * auto heap = std::make_unique<DescriptorHeap>();
         * heap->type = type;
         * heap->capacity = capacity;
         * heap->used = 0;
         *
         * // 1. 设置堆描述
         * D3D12_DESCRIPTOR_HEAP_DESC desc = {};
         * desc.NumDescriptors = capacity;
         * desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
         * desc.Type = (type == HeapType::CBV_SRV_UAV) ?
         *             D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV :
         *             D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
         *
         * // 2. 创建D3D12堆
         * auto device = D3D12RenderSystem::GetDevice();
         * HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap->heap));
         *
         * // 3. 获取描述符大小和起始句柄
         * heap->descriptorSize = device->GetDescriptorHandleIncrementSize(desc.Type);
         * heap->cpuStart = heap->heap->GetCPUDescriptorHandleForHeapStart();
         * heap->gpuStart = heap->heap->GetGPUDescriptorHandleForHeapStart();
         * ```
         */
        std::unique_ptr<DescriptorHeap> CreateDescriptorHeap(HeapType type, uint32_t capacity);

        /**
         * @brief 分配指定类型的索引
         * @param heapType 堆类型
         * @return 分配的索引，失败返回UINT32_MAX
         *
         * 实现指导:
         * ```cpp
         * // 1. 根据堆类型选择对应的使用状态数组
         * auto& usedArray = (heapType == HeapType::CBV_SRV_UAV) ?
         *                   m_cbvSrvUavUsed : m_samplerUsed;
         * auto& nextFreeIndex = (heapType == HeapType::CBV_SRV_UAV) ?
         *                       m_nextFreeCbvSrvUav : m_nextFreeSampler;
         *
         * // 2. 从nextFreeIndex开始搜索空闲位置 (优化性能)
         * for (uint32_t i = nextFreeIndex; i < usedArray.size(); ++i) {
         *     if (!usedArray[i]) {
         *         usedArray[i] = true;
         *         nextFreeIndex = i + 1;  // 下次从后面开始搜索
         *         UpdateStats(heapType, +1);
         *         return i;
         *     }
         * }
         *
         * // 3. 如果从nextFreeIndex没找到，从头开始搜索
         * for (uint32_t i = 0; i < nextFreeIndex; ++i) {
         *     if (!usedArray[i]) {
         *         usedArray[i] = true;
         *         nextFreeIndex = i + 1;
         *         UpdateStats(heapType, +1);
         *         return i;
         *     }
         * }
         * ```
         */
        uint32_t AllocateIndex(HeapType heapType);

        /**
         * @brief 释放指定类型的索引
         * @param heapType 堆类型
         * @param index 要释放的索引
         */
        void FreeIndex(HeapType heapType, uint32_t index);

        /**
         * @brief 查询硬件支持的最大描述符数量
         * @param heapType 堆类型
         * @return 最大支持数量
         */
        uint32_t QueryMaxDescriptorCount(HeapType heapType) const;

        /**
         * @brief 更新使用统计
         * @param heapType 堆类型
         * @param delta 变化量（+1分配，-1释放）
         */
        void UpdateStats(HeapType heapType, int32_t delta);

        // 禁用拷贝，支持移动
        DescriptorHeapManager(const DescriptorHeapManager&)            = delete;
        DescriptorHeapManager& operator=(const DescriptorHeapManager&) = delete;
        DescriptorHeapManager(DescriptorHeapManager&&)                 = default;
        DescriptorHeapManager& operator=(DescriptorHeapManager&&)      = default;
    };
} // namespace enigma::graphic
