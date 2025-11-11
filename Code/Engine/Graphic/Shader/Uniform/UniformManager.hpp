#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <typeindex>

#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/IntVec4.hpp"

// 引入 Uniform 结构体 (48 bytes Root Constants 架构)
#include "RootConstants.hpp"
#include "ColorTargetsIndexBuffer.hpp"      //  colortex0-15 (Main/Alt)
#include "DepthTexturesIndexBuffer.hpp"      //  新增 (depthtex0/1/2)
#include "ShadowColorIndexBuffer.hpp"       //  新增 (shadowcolor0-7)
#include "ShadowTexturesIndexBuffer.hpp"     //  新增 (shadowtex0/1)
#include "CustomImageIndexBuffer.hpp"        //  新增 自定义材质槽位 (customImage0-15)
#include "CameraAndPlayerUniforms.hpp"
// [REMOVED] #include "PlayerStatusUniforms.hpp"  // 已被 PerObjectUniforms 替代
#include "ScreenAndSystemUniforms.hpp"
#include "IDUniforms.hpp"
#include "WorldAndWeatherUniforms.hpp"
#include "BiomeAndDimensionUniforms.hpp"
#include "RenderingUniforms.hpp"
#include "MatricesUniforms.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"

namespace enigma::graphic
{
    // Forward declaration
    class D12Buffer;

    /**
     * @brief UpdateFrequency - Buffer更新频率分类 
     *
     * 教学要点:
     * 1. 根据更新频率优化内存分配策略
     * 2. PerObject需要Ring Buffer × 10000 (最高频率)
     * 3. PerPass需要Ring Buffer × 20 (中等频率)
     * 4. PerFrame和Static无需Ring Buffer × 1 (低频率)
     *
     * 使用场景:
     * - PerObject: MatricesUniforms (每次Draw更新)
     * - PerPass: ColorTargets, DepthTextures, ShadowColor (每个Pass更新)
     * - PerFrame: 其余10个Buffer (每帧更新一次)
     * - Static: 静态数据 (几乎不更新)
     */
    enum class UpdateFrequency
    {
        PerObject, // 每次Draw更新, Ring Buffer × 10000
        PerPass, // 每个Pass更新, Ring Buffer × 20
        PerFrame, // 每帧更新一次, × 1
        Static // 静态数据, × 1
    };

    /**
     * @brief PerObjectBufferState - Ring Buffer管理状态 
     *
     * 教学要点:
     * 1. **256字节对齐**: elementSize = (sizeof(T) + 255) & ~255
     * 2. **持久映射**: mappedData避免每次Map/Unmap开销
     * 3. **Delayed Fill**: lastUpdatedValue缓存最后更新值,避免内存浪费
     * 4. **Ring Buffer索引**: currentIndex % maxCount实现循环覆写
     *
     * 内存布局示例:
     * - MatricesUniforms: elementSize=1280 bytes (对齐后), maxCount=10000
     * - 总大小: 1280 × 10000 = 12.8 MB
     *
     * 使用流程:
     * 1. 构造时创建GPU Buffer并持久映射
     * 2. 每次UploadBuffer()调用GetCurrentIndex()获取写入位置
     * 3. 使用GetDataAt(index)获取写入地址
     * 4. 更新lastUpdatedValue和lastUpdatedIndex
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
         *
         * 教学要点: 使用指针算术,跳过index个elementSize字节
         */
        void* GetDataAt(size_t index);

        /**
         * @brief 获取当前应该使用的索引
         * @return 当前索引值
         *
         * 教学要点:
         * - PerObject: 使用Ring Buffer,返回 currentIndex % maxCount
         * - 其他频率: 单索引,始终返回0
         */
        size_t GetCurrentIndex() const;
    };

    /**
     * @brief Uniform管理器 - 完整 Iris 纹理系统架构 (48 bytes Root Constants) 🔥
     *
     * 核心架构设计 (基于 Iris 官方分类 + Bindless 优化 + 完整纹理支持):
     * 1. Root Constants = 48 bytes (12个uint32_t索引) 
     * 2. 12个 GPU 资源索引 (8个Uniform + 4个纹理Buffer + 1个直接纹理)
     * 3. 完整 Iris 纹理系统: colortex0-15 + depthtex0/1/2 + shadowcolor0-7 + shadowtex0/1 + noisetex
     * 4. **Fluent Builder + std::function Supplier模式** (模仿Iris设计) 
     *
     * 教学要点:
     * 1. SM6.6 Bindless架构: Root Signature极简化,无Descriptor Table
     * 2. 48字节限制: D3D12_MAX_ROOT_COST = 64 DWORDS (256 bytes总预算, Root Constants占18.75%)
     * 3. Supplier模式: 使用std::function懒加载,按需自动获取最新值
     * 4. Fluent Builder: 链式调用,代码优雅紧凑
     * 5. 激进合并方案: ShadowBuffer 合并 shadowcolor + shadowtex (80 bytes buffer, 4 bytes 索引)
     *
     * Iris官方分类 (https://shaders.properties/current/reference/uniforms/overview/):
     * - Camera/Player Uniforms      (CameraAndPlayerUniforms)
     * - Player Status Uniforms      (PlayerStatusUniforms)
     * - Screen/System Uniforms      (ScreenAndSystemUniforms)
     * - ID Uniforms                 (IDUniforms)
     * - World/Weather Uniforms      (WorldAndWeatherUniforms)
     * - Biome/Dimension Uniforms    (BiomeAndDimensionUniforms)
     * - Rendering Uniforms          (RenderingUniforms)
     * - Matrices                    (MatricesUniforms)
     *
     * 性能优势:
     * - Root Signature切换: 从1000次/帧降至1次/帧 (99.9%优化)
     * - 资源容量: 从数千提升至1,000,000+ (100倍提升)
     * - Supplier按需执行: 只在SyncToGPU()时调用,避免无效计算
     * - unordered_map查找: O(1)常数时间,只在注册时执行一次
     *
     * 使用流程 (两种模式):
     *
     * **模式1: 引擎内部使用 - Supplier自动拉取** 🔥
     * ```cpp
     * // RAII: 构造即初始化,无需手动Initialize()
     * UniformManager uniformMgr;
     *
     * // Fluent Builder注册Supplier (引擎内部组件可访问系统状态)
     * uniformMgr
     *     .Uniform3f("cameraPosition", []() { return CameraSystem::GetPosition(); })
     *     .Uniform2i("eyeBrightness", []() { return PlayerSystem::GetEyeBrightness(); })
     *     .Uniform1f("rainStrength", []() { return WeatherSystem::GetRainStrength(); })
     *     .UniformMat4("gbufferModelView", []() { return CameraSystem::GetModelViewMatrix(); });
     *
     * // 每帧调用SyncToGPU(),自动执行所有Supplier并上传到GPU
     * uniformMgr.SyncToGPU();
     * ```
     *
     * **模式2: 游戏侧使用 - 直接值推送** 🔥
     * ```cpp
     * // RAII: 构造即初始化
     * UniformManager uniformMgr;
     *
     * // 游戏侧直接推送数据 (完全解耦,无需访问引擎内部系统)
     * uniformMgr
     *     .Uniform3f("cameraPosition", Vec3(1.0f, 2.0f, 3.0f))
     *     .Uniform1f("rainStrength", 0.8f)
     *     .Uniform1i("worldTime", 6000)
     *     .UniformMat4("gbufferModelView", myViewMatrix);
     *
     * // 直接值模式已标记脏,SyncToGPU()只上传脏Buffer (无Supplier调用)
     * uniformMgr.SyncToGPU();
     * ```
     *
     * **混合模式: 两种方式可以同时使用** 🔥
     * ```cpp
     * uniformMgr
     *     // 引擎内部自动拉取
     *     .Uniform1f("frameTimeCounter", []() { return Clock::GetTime(); })
     *     // 游戏侧主动推送
     *     .Uniform3f("cameraPosition", gameCamera.GetPosition())
     *     .Uniform1f("rainStrength", gameWeather.GetRainStrength());
     *
     * uniformMgr.SyncToGPU();
     * ```
     *
     * // 传递到GPU (只需48字节Root Constants) 
     * bindlessRootSignature->SetRootConstants(
     *     commandList,
     *     uniformMgr.GetRootConstants(),
     *     12,  // 12 DWORDs 
     *     0    // 偏移量0
     * );
     * ```
     *
     * HLSL访问示例:
     * ```hlsl
     * cbuffer RootConstants : register(b0, space0)
     * {
     *     // Uniform Buffers (32 bytes)
     *     uint cameraAndPlayerBufferIndex;      // Offset 0
     *     uint playerStatusBufferIndex;         // Offset 4
     *     uint screenAndSystemBufferIndex;      // Offset 8
     *     uint idBufferIndex;                   // Offset 12
     *     uint worldAndWeatherBufferIndex;      // Offset 16
     *     uint biomeAndDimensionBufferIndex;    // Offset 20
     *     uint renderingBufferIndex;            // Offset 24
     *     uint matricesBufferIndex;             // Offset 28
     *
     *     // Texture Buffers (12 bytes) 
     *     uint colorTargetsBufferIndex;         // Offset 32 (colortex0-15)
     *     uint depthTexturesBufferIndex;        // Offset 36 (depthtex0/1/2)
     *     uint shadowBufferIndex;               // Offset 40 (shadowcolor0-7 + shadowtex0/1)
     *
     *     // Direct Texture (4 bytes) 
     *     uint noiseTextureIndex;               // Offset 44 (noisetex)
     * };
     *
     * // Uniform访问示例
     * StructuredBuffer<CameraAndPlayerUniforms> cameraBuffer =
     *     ResourceDescriptorHeap[cameraAndPlayerBufferIndex];
     * float3 cameraPos = cameraBuffer[0].cameraPosition;
     *
     * // 纹理访问示例 (通过 Common.hlsl 宏)
     * Texture2D colortex0 = GetRenderTarget(0);  // 自动处理 Main/Alt
     * Texture2D<float> depthtex0 = GetDepthTex0();
     * Texture2D shadowcolor0 = GetShadowColor(0);
     * Texture2D noisetex = ResourceDescriptorHeap[noiseTextureIndex];
     * ```
     */
    class UniformManager
    {
    public:
        /**
         * @brief 构造函数 - RAII自动初始化 🔥
         *
         * 教学要点 (遵循RAII原则):
         * 1. 初始化所有 12 个 CPU 端结构体为默认值 
         * 2. 构建字段映射表 (unordered_map, BuildFieldMap())
         * 3. 创建 12 个 GPU StructuredBuffer 
         * 4. 上传初始数据到GPU
         * 5. 注册到Bindless系统,获取 12 个索引并更新 Root Constants (48 bytes) 
         * 6. 构造完成即可用,无需手动Initialize()
         *
         * RAII优势:
         * - 资源获取即初始化 (Resource Acquisition Is Initialization)
         * - 异常安全: 构造失败会抛出异常,析构函数保证清理
         * - 避免"未初始化"状态: 对象创建完成就处于可用状态
         */
        UniformManager();

        /**
         * @brief 析构函数 - RAII自动释放资源 🔥
         *
         * 教学要点:
         * 1. 自动注销 12 个 Bindless 索引 
         * 2. 释放 12 个 GPU StructuredBuffer 
         * 3. RAII原则 - 无需手动Shutdown(),自动资源管理
         */
        ~UniformManager();

        /**
         * @brief 根据Root Slot获取Buffer状态 (Phase 1: Public API)
         * @param rootSlot Root Signature Slot编号 (0-13)
         * @return 对应的PerObjectBufferState指针，未找到返回nullptr
         *
         * 教学要点:
         * 1. DrawVertexArray需要访问此方法获取Matrices Buffer状态
         * 2. Phase 1只实现Slot 7 (Matrices)
         * 3. Phase 2会扩展到所有14个Slot
         *
         * 使用场景:
         * - Delayed Fill: 检查当前索引是否已更新
         * - Root CBV绑定: 计算GPU虚拟地址
         */
        PerObjectBufferState* GetBufferStateBySlot(uint32_t rootSlot);

        // ========================================================================
        // TypeId-based Buffer注册和上传API (Phase 1)  [PUBLIC]
        // ========================================================================

        /**
         * @brief 注册类型化Buffer - 类型安全API
         * @tparam T Buffer数据类型 (例如: MatricesUniforms)
         * @param frequency Buffer更新频率
         * @param maxDraws PerObject模式的最大Draw数量 (默认10000)
         *
         * 教学要点:
         * 1. 使用std::type_index作为Key,实现类型安全
         * 2. 自动计算256字节对齐: (sizeof(T) + 255) & ~255
         * 3. 根据UpdateFrequency分配合理Buffer大小:
         *    - PerObject: maxDraws (默认10000)
         *    - PerPass: 20 (保守估计)
         *    - PerFrame/Static: 1
         * 4. 使用D12Buffer创建Upload Heap并持久映射
         * 5. 防止重复注册 (同一类型只能注册一次)
         *
         * 使用示例:
         * ```cpp
         * uniformMgr->RegisterBuffer<MatricesUniforms>(UpdateFrequency::PerObject);
         * uniformMgr->RegisterBuffer<CameraAndPlayerUniforms>(UpdateFrequency::PerFrame);
         * ```
         */
        template <typename T>
        void RegisterBuffer(UpdateFrequency frequency, size_t maxDraws = 10000);

        /**
         * @brief 上传数据到类型化Buffer
         * @tparam T Buffer数据类型 (必须与RegisterBuffer相同)
         * @param data 要上传的数据
         *
         * 教学要点:
         * 1. 使用std::type_index查找对应Buffer
         * 2. 计算当前写入索引 (Ring Buffer模式下会循环)
         * 3. 直接memcpy到持久映射的内存
         * 4. 更新lastUpdatedValue缓存 (用于Delayed Fill)
         *
         * 使用示例:
         * ```cpp
         * MatricesUniforms matrices;
         * matrices.gbufferModelView = viewMatrix;
         * uniformMgr->UploadBuffer(matrices);
         * ```
         */
        template <typename T>
        void UploadBuffer(const T& data);

        /**
         * @brief 获取当前Draw计数 (Phase 1)
         * @return 当前帧已执行的Draw Call数量
         *
         * 教学要点:
         * - 用于计算Ring Buffer索引: currentDrawCount % maxCount
         * - DrawVertexArray调用此方法获取当前索引
         */
        size_t GetCurrentDrawCount() const { return m_currentDrawCount; }

        /**
         * @brief 递增Draw计数 (Phase 1)
         *
         * 教学要点:
         * - 每次Draw Call后调用,递增计数
         * - 下一次Draw Call会使用新的索引
         */
        void IncrementDrawCount() { m_currentDrawCount++; }

        /**
         * @brief 重置Draw计数 (每帧调用)
         *
         * 教学要点:
         * 1. 在BeginFrame()中调用,重置当前帧的Draw计数为0
         * 2. 配合Ring Buffer实现索引管理
         * 3. Phase 2会扩展为重置所有Buffer的currentIndex
         */
        void ResetDrawCount()
        {
            m_currentDrawCount = 0;
            // Phase 2: 重置所有Buffer的currentIndex
        }

    private:
        // ========================================================================
        // Root CBV架构 - Ring Buffer管理 
        // ========================================================================

        /**
         * @brief PerObject Buffer管理容器
         *
         * 教学要点:
         * 1. 使用std::type_index作为Key实现类型安全
         * 2. 存储所有需要Ring Buffer管理的Buffer (例如: MatricesUniforms)
         * 3. 每个类型对应一个PerObjectBufferState状态
         *
         * 使用示例:
         * ```cpp
         * m_perObjectBuffers[typeid(MatricesUniforms)] = state;
         * ```
         */
        std::unordered_map<std::type_index, PerObjectBufferState> m_perObjectBuffers;


        /**
         * @brief 当前帧Draw计数
         *
         * 教学要点:
         * 1. 用于追踪当前帧已执行的Draw Call数量
         * 2. 配合Ring Buffer实现索引管理
         * 3. 每帧重置为0 (在BeginFrame()中)
         * 4. 每次Draw Call递增 (在UploadBuffer()中)
         */
        size_t m_currentDrawCount = 0;

        // 禁用拷贝 (遵循RAII原则)
        UniformManager(const UniformManager&)            = delete;
        UniformManager& operator=(const UniformManager&) = delete;
    };

    // ========================================================================
    // Template Method 实现 (必须在头文件中定义)
    // ========================================================================

    template <typename T>
    void UniformManager::RegisterBuffer(UpdateFrequency frequency, size_t maxDraws)
    {
        std::type_index typeId = std::type_index(typeid(T));

        // 防止重复注册
        if (m_perObjectBuffers.find(typeId) != m_perObjectBuffers.end())
        {
            LogWarn(core::LogRenderer, "Buffer already registered: %s", typeid(T).name());
            return;
        }

        // 计算256字节对齐的元素大小
        size_t rawSize     = sizeof(T);
        size_t alignedSize = (rawSize + 255) & ~255; // 256字节对齐

        // 根据频率决定Buffer大小
        size_t count = 1;
        switch (frequency)
        {
        case UpdateFrequency::PerObject:
            count = maxDraws; // 10000
            break;
        case UpdateFrequency::PerPass:
            count = 20; // 保守估计
            break;
        case UpdateFrequency::PerFrame:
        case UpdateFrequency::Static:
            count = 1;
            break;
        }

        // 创建GPU Buffer (Upload Heap, persistent mapping)
        size_t totalSize = alignedSize * count;

        BufferCreateInfo createInfo;
        createInfo.size         = totalSize;
        createInfo.usage        = BufferUsage::ConstantBuffer;
        createInfo.memoryAccess = MemoryAccess::CPUToGPU; // Upload Heap
        createInfo.initialData  = nullptr;
        createInfo.debugName    = typeid(T).name();

        auto gpuBuffer = new D12Buffer(createInfo);

        // 持久映射
        void* mappedData = gpuBuffer->MapPersistent();
        if (!mappedData)
        {
            core::LogError(core::LogRenderer, "Failed to map buffer: %s", typeid(T).name());
            delete gpuBuffer;
            return;
        }

        // 创建状态对象
        PerObjectBufferState state;
        state.gpuBuffer    = std::unique_ptr<D12Buffer>(gpuBuffer);
        state.mappedData   = mappedData;
        state.elementSize  = alignedSize;
        state.maxCount     = count;
        state.frequency    = frequency;
        state.currentIndex = 0;
        state.lastUpdatedValue.resize(alignedSize, 0); // 初始化为0
        state.lastUpdatedIndex = SIZE_MAX;

        m_perObjectBuffers[typeId] = std::move(state);

        LogInfo(core::LogRenderer, "Registered Buffer: type=%s, frequency=%d, size=%zu, count=%zu",
                typeid(T).name(), static_cast<int>(frequency), alignedSize, count);
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
