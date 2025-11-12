#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>

#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/Buffer/BufferHelper.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"

namespace enigma::graphic
{
    // Forward declaration
    class D12Buffer;

    /**
     * @brief Buffer更新频率分类，决定Ring Buffer大小和内存分配策略
     *
     * 频率决定Buffer容量:
     * - PerObject: 10000个元素 (每次Draw更新)
     * - PerPass: 20个元素 (每个Pass更新)
     * - PerFrame: 1个元素 (每帧更新)
     * - Static: 1个元素 (静态数据)
     */
    enum class UpdateFrequency
    {
        PerObject, // 每次Draw更新, Ring Buffer × 10000
        PerPass, // 每个Pass更新, Ring Buffer × 20
        PerFrame, // 每帧更新一次, × 1
        Static // 静态数据, × 1
    };

    /**
     * @brief Ring Buffer管理状态
     *
     * 关键特性:
     * - 256字节对齐: elementSize按D3D12要求对齐
     * - 持久映射: mappedData指针持续有效，避免Map/Unmap开销
     * - Delayed Fill: lastUpdatedValue缓存值，检测重复上传
     * - Ring Buffer: currentIndex循环使用maxCount个槽位
     */
    struct PerObjectBufferState
    {
        std::unique_ptr<D12Buffer> gpuBuffer; // GPU Buffer资源
        void*                      mappedData  = nullptr; // 持久映射指针
        size_t                     elementSize = 0; // 256字节对齐的元素大小
        size_t                     maxCount    = 0; // 最大元素数量
        UpdateFrequency            frequency; // 更新频率
        size_t                     currentIndex = 0; // 当前写入索引
        std::vector<uint8_t>       lastUpdatedValue; // 缓存最后更新值
        size_t                     lastUpdatedIndex = SIZE_MAX; // 最后更新的索引

        /**
         * @brief 获取指定索引的数据地址
         * @param index 索引值
         * @return 指向数据的void*指针
         */
        void* GetDataAt(size_t index);

        /**
         * @brief 获取当前应该使用的索引
         * @return PerObject频率返回currentIndex % maxCount，其他频率返回0
         */
        size_t GetCurrentIndex() const;
    };

    /**
     * @brief Uniform管理器 - 基于SM6.6 Bindless架构的Constant Buffer管理
     *
     * 架构特性:
     * - 使用TypeId作为Key，类型安全的Buffer注册和上传
     * - Ring Buffer支持高频更新（PerObject可达10000次/帧）
     * - 持久映射减少CPU开销
     * - 按UpdateFrequency自动分类和绑定
     * - 支持slot 0-14引擎保留，slot >=15用户自定义
     *
     * 使用示例:
     * ```cpp
     * // 注册Buffer
     * uniformMgr->RegisterBuffer<MatricesUniforms>(7, UpdateFrequency::PerObject);
     * uniformMgr->RegisterBuffer<CameraAndPlayerUniforms>(0, UpdateFrequency::PerFrame);
     *
     * // 上传数据
     * MatricesUniforms matrices;
     * matrices.gbufferModelView = viewMatrix;
     * uniformMgr->UploadBuffer(matrices);
     *
     * // 自动绑定所有PerObject Buffer
     * auto& slots = uniformMgr->GetSlotsByFrequency(UpdateFrequency::PerObject);
     * for (uint32_t slot : slots)
     * {
     *     auto* state = uniformMgr->GetBufferStateBySlot(slot);
     *     // 绑定Root CBV...
     * }
     * ```
     */
    class UniformManager
    {
    public:
        /**
         * @brief 构造函数 - RAII自动初始化
         *
         * 对象创建完成即可用，无需手动Initialize()
         */
        UniformManager();

        /**
         * @brief 析构函数 - RAII自动释放资源
         *
         * 自动清理所有GPU Buffer和映射内存
         */
        ~UniformManager();

        /**
         * @brief 根据Root Slot获取Buffer状态
         * @param rootSlot Root Signature Slot编号
         * @return 对应的PerObjectBufferState指针，未找到返回nullptr
         *
         * 用于DrawVertexArray获取Buffer状态并计算GPU虚拟地址
         */
        PerObjectBufferState* GetBufferStateBySlot(uint32_t rootSlot);

        // ========================================================================
        // TypeId-based Buffer registration and upload API [PUBLIC]
        // ========================================================================

        /**
         * @brief 注册类型化Buffer
         * @tparam T Buffer数据类型 (例如: MatricesUniforms)
         * @param registerSlot Root Signature Slot编号 (0-14引擎保留, >=15用户自定义)
         * @param frequency Buffer更新频率
         * @param maxDraws PerObject模式的最大Draw数量 (默认10000)
         *
         * 创建GPU Buffer并持久映射，自动计算256字节对齐大小。
         * 根据frequency分配容量：PerObject=maxDraws, PerPass=20, PerFrame/Static=1
         *
         * @note 防止重复注册，同一类型或slot只能注册一次
         */
        template <typename T>
        void RegisterBuffer(uint32_t registerSlot, UpdateFrequency frequency, size_t maxDraws = 10000);

        /**
         * @brief 上传数据到类型化Buffer
         * @tparam T Buffer数据类型 (必须与RegisterBuffer相同)
         * @param data 要上传的数据
         *
         * 自动计算Ring Buffer索引并memcpy到持久映射内存，更新lastUpdatedValue缓存
         */
        template <typename T>
        void UploadBuffer(const T& data);

        /**
         * @brief 获取当前Draw计数
         * @return 当前帧已执行的Draw Call数量，用于计算Ring Buffer索引
         */
        size_t GetCurrentDrawCount() const { return m_currentDrawCount; }

        /**
         * @brief 递增Draw计数
         *
         * 每次Draw Call后调用，下一次Draw使用新索引
         */
        void IncrementDrawCount() { m_currentDrawCount++; }

        /**
         * @brief 重置Draw计数
         *
         * 每帧调用，在BeginFrame()中重置为0
         */
        void ResetDrawCount()
        {
            m_currentDrawCount = 0;
        }

        // ========================================================================
        // Slot management query API [PUBLIC]
        // ========================================================================

        /**
         * @brief 根据TypeId查询已注册的slot
         * @tparam T Buffer数据类型
         * @return 已注册的slot编号，未注册返回UINT32_MAX
         */
        template <typename T>
        uint32_t GetRegisteredSlot() const
        {
            auto typeId = std::type_index(typeid(T));
            auto it     = m_typeToSlotMap.find(typeId);
            return (it != m_typeToSlotMap.end()) ? it->second : UINT32_MAX;
        }

        /**
         * @brief 检查slot是否已被注册
         * @param slot slot编号
         * @return true=已注册，false=未注册
         */
        bool IsSlotRegistered(uint32_t slot) const;

        /**
         * @brief 获取指定更新频率的所有slot列表
         * @param frequency 更新频率 (PerObject, PerPass, PerFrame, Static)
         * @return const引用，指向slot列表。未找到返回空vector
         *
         * 用于DrawVertexArray自动绑定所有PerObject Buffer，或批量操作特定频率的Buffer
         */
        const std::vector<uint32_t>& GetSlotsByFrequency(UpdateFrequency frequency) const;

    private:
        // ========================================================================
        // Root CBV架构 - Ring Buffer管理 
        // ========================================================================

        // TypeId到Buffer状态的映射，实现类型安全的Buffer管理
        std::unordered_map<std::type_index, PerObjectBufferState> m_perObjectBuffers;

        // Slot到TypeId映射表，用于GetBufferStateBySlot()快速查找
        std::unordered_map<uint32_t, std::type_index> m_slotToTypeMap;

        // TypeId到Slot映射表，用于GetRegisteredSlot<T>()反向查询
        std::unordered_map<std::type_index, uint32_t> m_typeToSlotMap;

        // UpdateFrequency到Slot列表映射表，支持自动绑定机制
        // DrawVertexArray遍历PerObject的所有slot并自动绑定
        std::unordered_map<UpdateFrequency, std::vector<uint32_t>> m_frequencyToSlotsMap;

        // 当前帧Draw计数，用于Ring Buffer索引管理
        size_t m_currentDrawCount = 0;

        // 禁用拷贝 (遵循RAII原则)
        UniformManager(const UniformManager&)            = delete;
        UniformManager& operator=(const UniformManager&) = delete;
    };

    // ========================================================================
    // Template Method implementation (must be defined in the header file)
    // ========================================================================
    template <typename T>
    void UniformManager::RegisterBuffer(uint32_t registerSlot, UpdateFrequency frequency, size_t maxDraws)
    {
        std::type_index typeId = std::type_index(typeid(T));

        // [REQUIRED] prevents repeated registration of the same type
        if (m_perObjectBuffers.find(typeId) != m_perObjectBuffers.end())
        {
            LogWarn(LogRenderer, "Buffer already registered: %s", typeid(T).name());
            return;
        }

        // [REQUIRED] Prevent repeated registration of the same slot
        if (IsSlotRegistered(registerSlot))
        {
            LogError(LogRenderer, "Slot %u already registered! Cannot register type: %s",
                     registerSlot, typeid(T).name());
            ERROR_AND_DIE(Stringf("Slot %u already registered! Cannot register type: %s",registerSlot, typeid(T).name()))
        }

        // Calculate 256-byte aligned element size
        size_t rawSize     = sizeof(T);
        size_t alignedSize = (rawSize + 255) & ~255; // 256-byte alignment

        //Determine the Buffer size based on frequency
        size_t count = 1;
        switch (frequency)
        {
        case UpdateFrequency::PerObject:
            count = maxDraws; // 10000
            break;
        case UpdateFrequency::PerPass:
            count = 20; // conservative estimate
            break;
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static:
            count = 1;
            break;
        }

        // [FIX] Create GPU Buffer (Upload Heap, persistent mapping)
        // [IMPORTANT] Use D3D12RenderSystem::CreateBuffer and follow the four-layer architecture
        size_t totalSize = alignedSize * count;

        BufferCreateInfo createInfo;
        createInfo.size         = totalSize;
        createInfo.usage        = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess = MemoryAccess::CPUToGPU; // Upload Heap
        createInfo.initialData  = nullptr;
        createInfo.debugName    = typeid(T).name();

        // [FIX] Use D3D12RenderSystem::CreateBuffer (returns std::unique_ptr)
        auto gpuBuffer = D3D12RenderSystem::CreateBuffer(createInfo);
        if (!gpuBuffer)
        {
            LogError(core::LogRenderer, "Failed to create buffer: %s", typeid(T).name());
            ERROR_AND_DIE(Stringf("Failed to create buffer: %s", typeid(T).name()))
        }

        // 持久映射
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            LogError(core::LogRenderer, "Failed to map buffer: %s", typeid(T).name());
            return;
        }

        //Create state object
        PerObjectBufferState state;
        state.gpuBuffer    = std::move(gpuBuffer); // std::unique_ptr moves directly
        state.mappedData   = mappedData;
        state.elementSize  = alignedSize;
        state.maxCount     = count;
        state.frequency    = frequency;
        state.currentIndex = 0;
        state.lastUpdatedValue.resize(alignedSize, 0); // Initialized to 0
        state.lastUpdatedIndex = SIZE_MAX;

        m_perObjectBuffers[typeId] = std::move(state);

        // [REQUIRED] Synchronously update slot mapping
        // [FIX] std::type_index cannot be constructed by default, insert/emplace must be used
        m_slotToTypeMap.insert_or_assign(registerSlot, typeId);
        m_typeToSlotMap.insert_or_assign(typeId, registerSlot);

        // [NEW] Automatically classify into UpdateFrequency list
        m_frequencyToSlotsMap[frequency].push_back(registerSlot);

        LogInfo(core::LogRenderer,
                "Registered Buffer: type=%s, slot=%u, frequency=%d, size=%zu, count=%zu",
                typeid(T).name(), registerSlot, static_cast<int>(frequency), alignedSize, count);
    }

    template <typename T>
    void UniformManager::UploadBuffer(const T& data)
    {
        std::type_index typeId = std::type_index(typeid(T));

        auto it = m_perObjectBuffers.find(typeId);
        if (it == m_perObjectBuffers.end())
        {
            LogError(core::LogRenderer, "Buffer not registered: %s", typeid(T).name());
            return;
        }

        PerObjectBufferState& state = it->second;

        // 计算当前写入索引
        size_t writeIndex = state.GetCurrentIndex();

        // 写入数据到GPU Buffer
        void* destPtr = state.GetDataAt(writeIndex);
        memcpy(destPtr, &data, sizeof(T));

        // 更新缓存值 (用于Delayed Fill)
        memcpy(state.lastUpdatedValue.data(), &data, sizeof(T));
        state.lastUpdatedIndex = writeIndex;

        // 递增索引 (Ring Buffer模式)
        state.currentIndex++;
    }
} // namespace enigma::graphic
