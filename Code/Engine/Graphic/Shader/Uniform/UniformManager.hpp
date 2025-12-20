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
#include "Engine/Graphic/Shader/Uniform/UniformCommon.hpp"
#include "Engine/Graphic/Core/EnigmaGraphicCommon.hpp" // For MAX_DRAWS_PER_FRAME

namespace enigma::graphic
{
    // Forward declaration
    class D12Buffer;

    // [NOTE] PerObjectBufferState is now defined in UniformCommon.hpp

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

        /**
         * @brief Register typed Buffer (supports explicit space parameter)
         * @tparam T Buffer data type (for example: MatricesUniforms)
         * @param registerSlot Root Signature Slot number (0-14 engine reserved, 0-99 user-defined)
         * @param frequency Buffer update frequency
         * @param space Register Space (0=Engine Root CBV, 1=Custom Descriptor Table)
         * @param maxDraws The maximum number of Draws in PerObject mode (default 10000)
         *
         * [IMPORTANT] HLSL binding rules:
         * - space=0 (Engine): shader uses register(bN)
         * - space=1 (Custom): shader must use register(bN, space1)
         *
         * [NEW] Space parameter routing:
         * - space=0: Call RegisterEngineBuffer, require slot 0-14, use Root CBV binding
         * - space=1: Call RegisterCustomBuffer, allow any slot, use Descriptor Table binding
         *
         * @example C++ side registration (explicit space parameter)
         * @code
         * // Engine Buffer (space=0)
         * g_theUniformManager->RegisterBuffer<CameraUniforms>(0, UpdateFrequency::PerFrame);
         * // Equivalent to: RegisterBuffer<CameraUniforms>(0, PerFrame, 0);
         *
         * // Custom Buffer (space=1)
         * g_theUniformManager->RegisterBuffer<MyUniform>(88, UpdateFrequency::PerFrame, 1);
         * @endcode
         *
         * @example HLSL side declaration
         * @code
         * // space=0 (Engine Buffer): use register(bN)
         * cbuffer CameraUniforms : register(b0) { ... };
         *
         * // space=1 (Custom Buffer): space1 must be added
         * cbuffer MyUniform: register(b88, space1) { ... }; // [CORRECT]
         * cbuffer MyUniform: register(b88) { ... }; // [WRONG] space1 is missing
         * @endcode
         *
         * @throws UniformBufferException if slot/space validation fails or buffer creation fails
         * @throws DescriptorHeapException if descriptor allocation fails (fatal, should never happen)
         *
         * @warning backward compatibility: default space=0 ensures that existing calls do not need to be modified
         * @note space validation is performed at registration time with no runtime overhead
         */
        // [REFACTORED] maxDraws default changed from 10000 to MAX_DRAWS_PER_FRAME (64)
        // This ensures Ring Buffer index stays within valid range and prevents memory waste
        template <typename T>
        void RegisterBuffer(uint32_t registerSlot, UpdateFrequency frequency, uint32_t space = 0, size_t maxDraws = MAX_DRAWS_PER_FRAME);

        /**
         * @brief Upload data to typed Buffer
         * @tparam T Buffer data type (must be the same as RegisterBuffer)
         * @param data The data to be uploaded
         *
         * Automatically calculate Ring Buffer index and memcpy to persistent mapped memory, update lastUpdatedValue cache
         *
         * [IMPORTANT] Multi-draw data independence:
         * - Each UploadCustomBuffer call writes to a new Ring Buffer slice
         * - currentDrawIndex increments after each draw (via IncrementDrawCount)
         * - GPU reads from slice N while CPU uploads to slice N+1
         * - maxDraws parameter limits maximum draws per frame
         *
         *Example:
         * UploadBuffer(red); // Write to slice 0 based on type
         * Draw(); // GPU reads slice 0
         * IncrementDrawCount(); // currentDrawIndex: 0 -> 1
         *
         * UploadBuffer(green); // Write to slice 1
         * Draw(); // GPU reads slice 1
         * IncrementDrawCount(); // currentDrawIndex: 1 -> 2
         */
        template <typename T>
        void UploadBuffer(const T& data);

        /**
         * @brief Upload data to Buffer (explicit slot and space parameters)
         * @tparam T Buffer data type
         * @param data The data to be uploaded
         * @param slot HLSL register slot
         * @param space Register space (0=Engine Root CBV, 1=Custom Descriptor Table)
         *
         * [ADD] Allows slot and space to be specified directly, bypassing the TypeId search mechanism.
         * Suitable for scenarios where dynamic switching of Buffer targets is required.
         *
         * @throws UniformBufferException if space parameter is invalid
         *
         * @example
         * @code
         * // Upload to Engine Buffer (space=0)
         * uniformMgr->UploadBuffer(cameraData, 0, 0);
         *
         * // Upload to Custom Buffer (space=1)
         * uniformMgr->UploadBuffer(customData, 88, 1);
         * @endcode
         */
        template <typename T>
        void UploadBuffer(const T& data, uint32_t slot, uint32_t space);

        /**
         * @brief Get the current Draw count
         * @return The number of Draw Calls executed in the current frame, used to calculate the Ring Buffer index
         */
        size_t GetCurrentDrawCount() const { return m_currentDrawCount; }

        /**
         * @brief Increment Draw count
         *
         * Called after each Draw Call, the next Draw will use the new index
         */
        void IncrementDrawCount()
        {
            m_currentDrawCount++;

            // [ADD] Increment all Custom buffers' draw index
            for (auto& [slotId, state] : m_customBufferStates)
            {
                state->currentDrawIndex++;
            }
        }

        /**
         * @brief reset Draw count
         *
         * Called every frame, reset to 0 in BeginFrame()
         */
        void ResetDrawCount()
        {
            m_currentDrawCount = 0;

            // [ADD] Reset all Custom buffers' draw index
            for (auto& [slotId, state] : m_customBufferStates)
            {
                state->currentDrawIndex = 0;
            }
        }

        // ========================================================================
        // Slot management query API [PUBLIC]
        // ========================================================================

        /**
         * @brief Query registered slots based on TypeId
         * @tparam T Buffer data type
         * @return the registered slot number, if not registered, return UINT32_MAX
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
         * @brief Get a list of all slots with a specified update frequency
         * @param frequency update frequency (PerObject, PerPass, PerFrame, Static)
         * @return const reference, pointing to the slot list. Not found returns empty vector
         *
         * Used for DrawVertexArray to automatically bind all PerObject Buffers, or batch operate Buffers with a specific frequency
         */
        const std::vector<uint32_t>& GetSlotsByFrequency(UpdateFrequency frequency) const;

        // ========================================================================
        // [NEW] Ring Buffer offset管理 - Draw函数调用
        // ========================================================================

        /**
         * @brief updates the Ring Buffer offset of the specified frequency
         * @param frequency update frequency (PerObject, PerPass, PerFrame)
         *
         *Teaching points:
         * 1. The Draw function is called before drawing and updates the offset of all related Buffers.
         * 2. Integrate Delayed Fill mechanism: detect whether data changes and avoid repeated uploading
         * 3. Call different Internal methods according to the Buffer type:
         * - Root CBV: UpdateRootCBVOffset()
         * - Custom Buffer: UpdateDescriptorTableOffset()
         *
         * Reference: RendererSubsystem.cpp:2367-2400 - Delayed Fill implementation
         */
        void UpdateRingBufferOffsets(UpdateFrequency frequency);

        // ========================================================================
        // [NEW Phase 4] Public接口 - Custom Buffer和Engine Buffer地址获取
        // ========================================================================

        /**
         * @brief Get the GPU handle of Custom Buffer Descriptor Table (for Slot 15 binding)
         * @param ringIndex [FIX] Ring Descriptor Table index, each Draw uses a different index
         * @return GPU Descriptor Handle, pointing to baseHandle + ringIndex * MAX_CUSTOM_BUFFERS * descriptorSize
         *
         * [FIX] Ring Descriptor Table architecture:
         * - Use a different copy of the Descriptor Table for each Draw
         * - ringIndex = currentDrawCount % MAX_RING_FRAMES
         * - Solve the problem of multi-Draw data independence caused by CBV Descriptor coverage
         */
        D3D12_GPU_DESCRIPTOR_HANDLE GetCustomBufferDescriptorTableGPUHandle(uint32_t ringIndex = 0) const;

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

        // [NOTE] CustomBufferDescriptor is now defined in UniformCommon.hpp

        // Custom Buffer Descriptor映射表：slotId -> CustomBufferDescriptor
        std::unordered_map<uint32_t, CustomBufferDescriptor> m_customBufferDescriptors;

        // [NEW] Custom Buffer state management (space=1)
        // Key: slot number, Value: buffer state with Ring Buffer and Descriptor info
        std::unordered_map<uint32_t, std::unique_ptr<CustomBufferState>> m_customBufferStates;

        // [NEW] Custom Buffer Descriptor预分配池（100个连续Descriptor）
        // [RAII] 在构造函数中从GlobalDescriptorHeapManager批量分配，确保连续性
        std::vector<GlobalDescriptorHeapManager::DescriptorAllocation> m_customBufferDescriptorPool;

        // Custom Buffer Descriptor Table基础GPU句柄（Slot 15绑定）
        D3D12_GPU_DESCRIPTOR_HANDLE m_customBufferDescriptorTableBaseGPUHandle = {};

        // [FIX] Increment size of CBV/SRV/UAV Descriptor (used to calculate Ring Descriptor Table offset)
        UINT m_cbvSrvUavDescriptorIncrementSize = 0;

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
            uint32_t        slotId,
            std::type_index typeId,
            size_t          bufferSize,
            UpdateFrequency freq,
            size_t          maxDraws,
            uint32_t        space = 0
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
            uint32_t        slotId,
            std::type_index typeId,
            size_t          bufferSize,
            UpdateFrequency freq,
            size_t          maxDraws,
            uint32_t        space = 0
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
    void UniformManager::RegisterBuffer(uint32_t registerSlot, UpdateFrequency frequency,
                                        uint32_t space, size_t                 maxDraws)
    {
        try
        {
            // [REFACTOR] Validate space parameter - throw exception instead of return
            if (space != 0 && space != 1)
            {
                throw UniformBufferException("Invalid space parameter, must be 0 (Engine) or 1 (Custom)",
                                             registerSlot, space);
            }

            std::type_index typeId     = std::type_index(typeid(T));
            size_t          bufferSize = sizeof(T);

            // [NOTE] Duplicate registration is a warning, not an error
            if (m_perObjectBuffers.find(typeId) != m_perObjectBuffers.end())
            {
                LogWarn(LogUniform, "Buffer already registered: %s", typeid(T).name());
                return;
            }

            // [REFACTOR] Route by space parameter - internal methods may throw
            bool success = false;
            if (space == 0)
            {
                // Engine Buffer path (space=0, Root CBV)
                success = RegisterEngineBuffer(registerSlot, typeId, bufferSize, frequency, maxDraws, space);
            }
            else
            {
                // Custom Buffer path (space=1, Descriptor Table)
                success = RegisterCustomBuffer(registerSlot, typeId, bufferSize, frequency, maxDraws, space);
            }

            if (!success)
            {
                throw UniformBufferException("Internal registration failed", registerSlot, space);
            }

            // Record TypeId mapping on success
            m_typeToSlotMap[typeId] = registerSlot;
            m_slotToTypeMap.insert({registerSlot, typeId});
        }
        catch (const UniformBufferException& e)
        {
            // [ADD] Recoverable error - log and notify via ERROR_RECOVERABLE
            LogError(LogUniform, "RegisterBuffer failed: %s (slot=%u, space=%u)",
                     e.what(), e.GetSlot(), e.GetSpace());
            ERROR_RECOVERABLE(e.what());
            throw; // Re-throw for caller to handle
        }
        catch (const DescriptorHeapException& e)
        {
            // [ADD] Fatal error - descriptor exhaustion should never happen
            LogError(LogUniform, "RegisterBuffer descriptor allocation failed: %s (slot=%u, space=%u)",
                     e.what(), registerSlot, space);
            ERROR_AND_DIE(e.what());
        }
        catch (const std::exception& e)
        {
            // [ADD] Unexpected error - treat as fatal
            LogError(LogUniform, "RegisterBuffer unexpected error: %s", e.what());
            ERROR_AND_DIE(e.what());
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
            LogError(LogUniform, "Buffer not registered: %s", typeid(T).name());
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

    // ========================================================================
    // [ADD] UploadBuffer with explicit slot and space parameter
    // ========================================================================
    template <typename T>
    void UniformManager::UploadBuffer(const T& data, uint32_t slot, uint32_t space)
    {
        // [ADD] Validate space parameter
        if (space != 0 && space != 1)
        {
            LogError(LogUniform, "Invalid space parameter: %u (must be 0 or 1)", space);
            throw UniformBufferException("Invalid space parameter", slot, space);
        }

        // [ADD] Route to appropriate upload method based on space
        if (space == 0)
        {
            // Engine Buffer path (space=0, Root CBV)
            UploadEngineBuffer(slot, &data, sizeof(T));
        }
        else
        {
            // Custom Buffer path (space=1, Descriptor Table)
            UploadCustomBuffer(slot, &data, sizeof(T));
        }
    }
} // namespace enigma::graphic
