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
#include "Engine/Graphic/Shader/Uniform/UpdateFrequency.hpp"

namespace enigma::graphic
{
    // Forward declaration
    class D12Buffer;

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
         * [RAII] 对象创建完成即可用，自动完成以下初始化：
         * 1. 预分配Custom Buffer Descriptor池（100个连续Descriptor）
         * 2. 验证Descriptor连续性（Descriptor Table要求）
         * 3. 保存第一个Descriptor的GPU句柄作为Descriptor Table基址
         *
         * @note 如果初始化失败，会通过ERROR_AND_DIE()终止程序
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
        PerObjectBufferState* GetBufferStateBySlot(uint32_t rootSlot) const;

        // ========================================================================
        // TypeId-based Buffer registration and upload API [PUBLIC]
        // ========================================================================

        /**
         * @brief 注册类型化Buffer
         * @tparam T Buffer数据类型 (例如: MatricesUniforms)
         * @param registerSlot Root Signature Slot编号 (0-14引擎保留, 15-99用户自定义)
         * @param frequency Buffer更新频率
         * @param maxDraws PerObject模式的最大Draw数量 (默认10000)
         *
         * 创建GPU Buffer并持久映射，自动计算256字节对齐大小。
         * 根据frequency分配容量：PerObject=maxDraws, PerPass=20, PerFrame/Static=1
         *
         * @note [IMPORTANT] Shader绑定规则：
         *       - Slot 0-14:  使用Root CBV，shader中使用 register(bN)
         *       - Slot 15-99: 使用Descriptor Table，shader中必须使用 register(bN, space1)
         *
         * @example C++侧注册
         * @code
         * g_theUniformManager->RegisterBuffer<MyUniform>(88, UpdateFrequency::PerFrame);
         * @endcode
         *
         * @example HLSL侧声明（Slot >= 15时必须添加space1）
         * @code
         * // [CORRECT] Slot 88使用space1
         * cbuffer MyUniform : register(b88, space1) { float4 myData; };
         *
         * // [WRONG] 缺少space1会导致绑定失败
         * cbuffer MyUniform : register(b88) { float4 myData; };  // [BAD]
         * @endcode
         *
         * @warning 如果slot >= 15但shader中未使用space1，将导致绑定失败！
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

        // ========================================================================
        // [NEW] Ring Buffer offset管理 - Draw函数调用
        // ========================================================================

        /**
         * @brief 更新指定频率的Ring Buffer offset
         * @param frequency 更新频率 (PerObject, PerPass, PerFrame)
         *
         * 教学要点:
         * 1. Draw函数在绘制前调用，更新所有相关Buffer的offset
         * 2. 集成Delayed Fill机制：检测数据是否变化，避免重复上传
         * 3. 根据Buffer类型调用不同的Internal方法：
         *    - Root CBV: UpdateRootCBVOffset()
         *    - Custom Buffer: UpdateDescriptorTableOffset()
         *
         * 参考: RendererSubsystem.cpp:2367-2400 - Delayed Fill实现
         */
        void UpdateRingBufferOffsets(UpdateFrequency frequency);

        // ========================================================================
        // [NEW Phase 4] Public接口 - Custom Buffer和Engine Buffer地址获取
        // ========================================================================

        /**
         * @brief 获取Custom Buffer Descriptor Table的GPU句柄（用于Slot 15绑定）
         * @return GPU Descriptor Handle
         */
        D3D12_GPU_DESCRIPTOR_HANDLE GetCustomBufferDescriptorTableGPUHandle() const;

        /**
         * @brief 获取Engine Buffer的GPU虚拟地址（用于Root CBV绑定）
         * @param slotId Engine Buffer的Slot编号 (0-14)
         * @return GPU虚拟地址，如果Buffer不存在返回0
         */
        D3D12_GPU_VIRTUAL_ADDRESS GetEngineBufferGPUAddress(uint32_t slotId) const;

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

        // ========================================================================
        // [NEW] Custom Buffer Descriptor管理
        // ========================================================================

        /**
         * @brief Custom Buffer Descriptor分配记录
         *
         * 教学要点:
         * 1. 记录每个Custom Buffer的CBV Descriptor分配信息
         * 2. slotId范围：15-114（对应b15-b114）
         * 3. allocation包含CPU和GPU句柄，用于创建CBV和绑定
         */
        struct CustomBufferDescriptor
        {
            GlobalDescriptorHeapManager::DescriptorAllocation allocation; // Descriptor分配信息
            uint32_t                                          slotId; // Slot编号 (15-114)
            bool                                              isValid; // 有效性标记

            CustomBufferDescriptor() : slotId(0), isValid(false)
            {
            }
        };

        // Custom Buffer Descriptor映射表：slotId -> CustomBufferDescriptor
        std::unordered_map<uint32_t, CustomBufferDescriptor> m_customBufferDescriptors;

        // [NEW] Custom Buffer Descriptor预分配池（100个连续Descriptor）
        // [RAII] 在构造函数中从GlobalDescriptorHeapManager批量分配，确保连续性
        std::vector<GlobalDescriptorHeapManager::DescriptorAllocation> m_customBufferDescriptorPool;

        // Custom Buffer Descriptor Table基础GPU句柄（Slot 15绑定）
        D3D12_GPU_DESCRIPTOR_HANDLE m_customBufferDescriptorTableBaseGPUHandle = {};

        // 初始化状态标记
        bool m_initialized = false;

        // ========================================================================
        // [NEW] Buffer注册和上传内部实现
        // ========================================================================

        /**
         * @brief 注册引擎保留Buffer（Slot 0-14）
         * @param slotId Slot编号 (0-14)
         * @param typeId 类型ID
         * @param bufferSize 原始Buffer大小
         * @param freq 更新频率
         * @param maxDraws PerObject模式的最大Draw数量
         * @param space Register Space (默认0)
         * @return 成功返回true
         *
         * 实现要点:
         * 1. 验证Slot范围：必须在0-14之间
         * 2. 创建GPU Buffer并持久映射
         * 3. 创建PerObjectBufferState，复用现有架构
         * 4. 更新m_slotToTypeMap和m_typeToSlotMap
         */
        bool RegisterEngineBuffer(
            uint32_t             slotId,
            std::type_index      typeId,
            size_t               bufferSize,
            UpdateFrequency      freq,
            size_t               maxDraws,
            uint32_t             space = 0
        );

        /**
         * @brief 注册Custom Buffer（用户自定义Constant Buffer）
         * @param slotId Slot编号 (0-99)
         * @param typeId 类型ID
         * @param bufferSize 原始Buffer大小
         * @param freq 更新频率
         * @param maxDraws PerObject模式的最大Draw数量
         * @param space Register Space (默认0，内部强制使用space1)
         * @return 成功返回true
         *
         * @note [IMPORTANT] Shader绑定规则：
         *       - Slot 0-14:  使用Root CBV直接绑定，shader中使用 register(bN)
         *       - Slot 15-99: 使用Descriptor Table绑定，shader中必须使用 register(bN, space1)
         *
         * @example C++侧注册
         * @code
         * // 注册Custom Buffer到Slot 88
         * g_theUniformManager->RegisterBuffer<MyUniform>(88, UpdateFrequency::PerFrame, 1);
         * @endcode
         *
         * @example HLSL侧声明（Slot >= 15时必须添加space1）
         * @code
         * // [CORRECT] Slot 88使用space1
         * cbuffer MyUniform : register(b88, space1)
         * {
         *     float4 myData;
         * };
         *
         * // [WRONG] 缺少space1会导致绑定失败
         * cbuffer MyUniform : register(b88)  // [BAD] 错误！
         * {
         *     float4 myData;
         * };
         * @endcode
         *
         * @warning 如果slot >= 15但shader中未使用space1，将导致绑定失败！
         *
         * 实现要点:
         * 1. 验证Slot范围：必须在0-99范围内
         * 2. 创建GPU Buffer并持久映射
         * 3. 分配Custom CBV Descriptor（调用AllocateCustomBufferDescriptor）
         * 4. 创建CBV View，写入Descriptor
         * 5. 创建PerObjectBufferState，复用现有架构
         */
        bool RegisterCustomBuffer(
            uint32_t             slotId,
            std::type_index      typeId,
            size_t               bufferSize,
            UpdateFrequency      freq,
            size_t               maxDraws,
            uint32_t             space = 0
        );

        /**
         * @brief 上传数据到引擎保留Buffer
         * @param slotId Slot编号 (0-14)
         * @param data 数据指针
         * @param size 数据大小
         * @return 成功返回true
         */
        bool UploadEngineBuffer(uint32_t slotId, const void* data, size_t size);

        /**
         * @brief 上传数据到Custom Buffer
         * @param slotId Slot编号 (15-99，使用space1)
         * @param data 数据指针
         * @param size 数据大小
         * @return 成功返回true
         */
        bool UploadCustomBuffer(uint32_t slotId, const void* data, size_t size);

        // ========================================================================
        // [NEW] Ring Buffer offset更新内部实现
        // ========================================================================

        /**
         * @brief 更新Root CBV的offset（引擎Buffer）
         * @param slotId Slot编号 (0-14)
         * @param currentIndex 当前Ring Buffer索引
         *
         * 实现要点:
         * 1. 计算GPU虚拟地址：baseAddress + currentIndex * elementSize
         * 2. 调用CommandList->SetGraphicsRootConstantBufferView(slotId, address)
         */
        void UpdateRootCBVOffset(uint32_t slotId, size_t currentIndex);

        /**
         * @brief 更新Descriptor Table中的CBV offset（Custom Buffer）
         * @param slotId Slot编号 (15-99，使用space1)
         * @param currentIndex 当前Ring Buffer索引
         *
         * 实现要点:
         * 1. 计算Buffer的GPU虚拟地址：baseAddress + currentIndex * elementSize
         * 2. 获取Custom Buffer的CPU Descriptor Handle
         * 3. 创建D3D12_CONSTANT_BUFFER_VIEW_DESC
         * 4. 调用Device->CreateConstantBufferView()更新Descriptor
         */
        void UpdateDescriptorTableOffset(uint32_t slotId, size_t currentIndex);

        // ========================================================================
        // [NEW] Custom Buffer Descriptor管理辅助方法
        // ========================================================================

        /**
         * @brief 为Custom Buffer分配Descriptor
         * @param slotId Slot编号 (15-99，使用space1)
         * @return 成功返回true
         *
         * 实现要点:
         * 1. 调用GlobalDescriptorHeapManager::BatchAllocateCustomCbv(1)
         * 2. 创建CustomBufferDescriptor并存入m_customBufferDescriptors
         * 3. 如果是第一个Custom Buffer，保存DescriptorTable基址
         */
        bool AllocateCustomBufferDescriptor(uint32_t slotId);

        /**
         * @brief 释放Custom Buffer的Descriptor
         * @param slotId Slot编号 (15-99，使用space1)
         */
        void FreeCustomBufferDescriptor(uint32_t slotId);

        /**
         * @brief 获取指定Custom Buffer的CPU Descriptor句柄
         * @param slotId Slot编号 (15-99，使用space1)
         * @return CPU Descriptor Handle
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetCustomBufferCPUHandle(uint32_t slotId) const;

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
        std::type_index typeId     = std::type_index(typeid(T));
        size_t          bufferSize = sizeof(T);

        // [REQUIRED] prevents repeated registration of the same type
        if (m_perObjectBuffers.find(typeId) != m_perObjectBuffers.end())
        {
            LogWarn(LogRenderer, "Buffer already registered: %s", typeid(T).name());
            return;
        }

        // [NEW] 自动路由：根据slotId判断是引擎Buffer还是Custom Buffer
        bool success = false;
        if (BufferHelper::IsEngineReservedSlot(registerSlot))
        {
            // 引擎Buffer路径（Slot 0-14）
            success = RegisterEngineBuffer(registerSlot, typeId, bufferSize, frequency, maxDraws, 0);
        }
        else
        {
            // Custom Buffer路径（Slot 15-99，使用space1）
            success = RegisterCustomBuffer(registerSlot, typeId, bufferSize, frequency, maxDraws, 0);
        }

        if (success)
        {
            // 记录TypeId映射
            m_typeToSlotMap[typeId]       = registerSlot;
            m_slotToTypeMap.insert({registerSlot, typeId});
        }
    }

    template <typename T>
    void UniformManager::UploadBuffer(const T& data)
    {
        std::type_index typeId = std::type_index(typeid(T));

        // 获取已注册的slot
        auto slotIt = m_typeToSlotMap.find(typeId);
        if (slotIt == m_typeToSlotMap.end())
        {
            LogError(core::LogRenderer, "Buffer not registered: %s", typeid(T).name());
            return;
        }

        uint32_t slotId = slotIt->second;

        // [NEW] 自动路由：根据slotId判断是引擎Buffer还是Custom Buffer
        if (BufferHelper::IsEngineReservedSlot(slotId))
        {
            // 引擎Buffer路径（Slot 0-14）
            UploadEngineBuffer(slotId, &data, sizeof(T));
        }
        else
        {
            // Custom Buffer路径（Slot 15-99，使用space1）
            UploadCustomBuffer(slotId, &data, sizeof(T));
        }
    }
} // namespace enigma::graphic
