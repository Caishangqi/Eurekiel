#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <unordered_map>
#include <d3d12.h>

#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Math/Mat44.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/IntVec4.hpp"

// 引入9个独立的Uniform结构体
#include "RootConstants.hpp"
#include "RenderTargetsIndexBuffer.hpp"
#include "CameraAndPlayerUniforms.hpp"
#include "PlayerStatusUniforms.hpp"
#include "ScreenAndSystemUniforms.hpp"
#include "IDUniforms.hpp"
#include "WorldAndWeatherUniforms.hpp"
#include "BiomeAndDimensionUniforms.hpp"
#include "RenderingUniforms.hpp"
#include "MatricesUniforms.hpp"

namespace enigma::graphic
{
    /**
     * @brief Uniform管理器 - RENDERTARGETS混合架构 (44 bytes Root Constants) 🔥
     *
     * 核心架构设计 (基于Iris官方分类 + Bindless优化):
     * 1. Root Constants = 44 bytes (11个uint32_t索引)
     * 2. 9个独立GPU StructuredBuffer (8个Iris类别 + RenderTargetsBuffer)
     * 3. 所有大数据存储在GPU Buffer, Root Constants只传索引
     * 4. **Fluent Builder + std::function Supplier模式** (模仿Iris设计) ⭐
     *
     * 教学要点:
     * 1. SM6.6 Bindless架构: Root Signature极简化,无Descriptor Table
     * 2. 44字节限制: D3D12_MAX_ROOT_COST = 64 DWORDS (256 bytes总预算, Root Constants占17%)
     * 3. Supplier模式: 使用std::function懒加载,按需自动获取最新值
     * 4. Fluent Builder: 链式调用,代码优雅紧凑
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
     * // 传递到GPU (只需44字节Root Constants)
     * bindlessRootSignature->SetRootConstants(
     *     commandList,
     *     uniformMgr.GetRootConstants(),
     *     11,  // 11 DWORDs
     *     0    // 偏移量0
     * );
     * ```
     *
     * HLSL访问示例:
     * ```hlsl
     * cbuffer RootConstants : register(b0, space0)
     * {
     *     uint cameraAndPlayerBufferIndex;  // Offset 0
     *     // ... 其他10个索引
     * };
     *
     * StructuredBuffer<CameraAndPlayerUniforms> cameraBuffer =
     *     ResourceDescriptorHeap[cameraAndPlayerBufferIndex];
     * float3 cameraPos = cameraBuffer[0].cameraPosition;
     * ```
     */
    class UniformManager
    {
    public:
        /**
         * @brief 构造函数 - RAII自动初始化 🔥
         *
         * 教学要点 (遵循RAII原则):
         * 1. 初始化所有9个CPU端结构体为默认值
         * 2. 构建字段映射表 (unordered_map, BuildFieldMap())
         * 3. 创建9个GPU StructuredBuffer
         * 4. 上传初始数据到GPU
         * 5. 注册到Bindless系统,获取9个索引并更新Root Constants
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
         * 1. 自动注销9个Bindless索引
         * 2. 释放9个GPU StructuredBuffer
         * 3. RAII原则 - 无需手动Shutdown(),自动资源管理
         */
        ~UniformManager();

        // ========================================================================
        // Fluent Builder API - Uniform注册接口 (模仿Iris UniformHolder)
        // ========================================================================

        /**
         * @brief 注册float Uniform - Supplier版本 (引擎内部使用)
         * @param name Uniform名称 (例如: "rainStrength", "playerMood")
         * @param getter Supplier lambda (例如: []() { return 0.5f; })
         * @return *this (支持链式调用)
         *
         * 教学要点:
         * 1. Supplier模式: getter只在SyncToGPU()时调用
         * 2. 自动映射: "rainStrength" -> WorldAndWeatherUniforms.rainStrength
         * 3. 链式调用: 返回*this支持 .Uniform1f().Uniform2i()
         *
         * 使用场景: 引擎内部组件可访问系统状态
         * ```cpp
         * uniformMgr.Uniform1f("rainStrength", []() {
         *     return WeatherSystem::GetRainStrength();
         * });
         * ```
         */
        UniformManager& Uniform1f(const std::string& name, std::function<float()> getter);

        /**
         * @brief 注册float Uniform - 直接值版本 (游戏侧使用) 🔥
         * @param name Uniform名称 (例如: "rainStrength")
         * @param value 直接传入的float值
         * @return *this (支持链式调用)
         *
         * 教学要点:
         * 1. 游戏侧完全解耦: 不依赖引擎内部系统
         * 2. 立即更新: 直接写入CPU端结构体并标记脏
         * 3. 无Supplier开销: 跳过SyncToGPU()的Supplier调用
         *
         * 使用场景: 游戏侧主动推送数据到引擎
         * ```cpp
         * uniformMgr.Uniform1f("rainStrength", 0.8f);
         * uniformMgr.Uniform3f("cameraPosition", Vec3(1.0f, 2.0f, 3.0f));
         * ```
         */
        UniformManager& Uniform1f(const std::string& name, float value);

        /**
         * @brief 注册int Uniform - Supplier版本 (引擎内部使用)
         */
        UniformManager& Uniform1i(const std::string& name, std::function<int32_t()> getter);

        /**
         * @brief 注册int Uniform - 直接值版本 (游戏侧使用) 🔥
         */
        UniformManager& Uniform1i(const std::string& name, int32_t value);

        /**
         * @brief 注册bool Uniform - Supplier版本 (引擎内部使用)
         *
         * 教学要点: bool在GPU端存储为int32_t (HLSL bool兼容性)
         */
        UniformManager& Uniform1b(const std::string& name, std::function<bool()> getter);

        /**
         * @brief 注册bool Uniform - 直接值版本 (游戏侧使用) 🔥
         */
        UniformManager& Uniform1b(const std::string& name, bool value);

        /**
         * @brief 注册Vec2 Uniform - Supplier版本 (引擎内部使用)
         */
        UniformManager& Uniform2f(const std::string& name, std::function<Vec2()> getter);

        /**
         * @brief 注册Vec2 Uniform - 直接值版本 (游戏侧使用) 🔥
         */
        UniformManager& Uniform2f(const std::string& name, const Vec2& value);

        /**
         * @brief 注册IntVec2 Uniform - Supplier版本 (引擎内部使用)
         */
        UniformManager& Uniform2i(const std::string& name, std::function<IntVec2()> getter);

        /**
         * @brief 注册IntVec2 Uniform - 直接值版本 (游戏侧使用) 🔥
         */
        UniformManager& Uniform2i(const std::string& name, const IntVec2& value);

        /**
         * @brief 注册Vec3 Uniform - Supplier版本 (引擎内部使用)
         */
        UniformManager& Uniform3f(const std::string& name, std::function<Vec3()> getter);

        /**
         * @brief 注册Vec3 Uniform - 直接值版本 (游戏侧使用) 🔥
         *
         * 使用示例:
         * ```cpp
         * uniformMgr.Uniform3f("cameraPosition", Vec3(1.0f, 2.0f, 3.0f));
         * ```
         */
        UniformManager& Uniform3f(const std::string& name, const Vec3& value);

        /**
         * @brief 注册IntVec3 Uniform - Supplier版本 (引擎内部使用)
         */
        UniformManager& Uniform3i(const std::string& name, std::function<IntVec3()> getter);

        /**
         * @brief 注册IntVec3 Uniform - 直接值版本 (游戏侧使用) 🔥
         */
        UniformManager& Uniform3i(const std::string& name, const IntVec3& value);

        /**
         * @brief 注册Vec4 Uniform - Supplier版本 (引擎内部使用)
         */
        UniformManager& Uniform4f(const std::string& name, std::function<Vec4()> getter);

        /**
         * @brief 注册Vec4 Uniform - 直接值版本 (游戏侧使用) 🔥
         */
        UniformManager& Uniform4f(const std::string& name, const Vec4& value);

        /**
         * @brief 注册IntVec4 Uniform - Supplier版本 (引擎内部使用)
         */
        UniformManager& Uniform4i(const std::string& name, std::function<IntVec4()> getter);

        /**
         * @brief 注册IntVec4 Uniform - 直接值版本 (游戏侧使用) 🔥
         */
        UniformManager& Uniform4i(const std::string& name, const IntVec4& value);

        /**
         * @brief 注册Mat44 Uniform - Supplier版本 (引擎内部使用)
         *
         * 教学要点: 矩阵使用alignas(16)确保16字节对齐
         */
        UniformManager& UniformMat4(const std::string& name, std::function<Mat44()> getter);

        /**
         * @brief 注册Mat44 Uniform - 直接值版本 (游戏侧使用) 🔥
         */
        UniformManager& UniformMat4(const std::string& name, const Mat44& value);

        // ========================================================================
        // RenderTargetsIndexBuffer 专用API
        // ========================================================================

        /**
         * @brief 更新RenderTargets读取索引 (colortex0-15)
         * @param readIndices 读取索引数组 (16个)
         *
         * 教学要点:
         * 1. Main/Alt Ping-Pong机制: readIndices指向当前作为源的纹理集
         * 2. 直接修改m_renderTargetsBuffer.readIndices
         */
        void UpdateRenderTargetsReadIndices(const uint32_t readIndices[16]);

        /**
         * @brief 更新RenderTargets写入索引 (colortex0-15, UAV扩展预留)
         * @param writeIndices 写入索引数组 (16个)
         */
        void UpdateRenderTargetsWriteIndices(const uint32_t writeIndices[16]);

        /**
         * @brief Flip RenderTargets (Main ↔ Alt)
         * @param mainIndices Main纹理索引数组 (16个)
         * @param altIndices Alt纹理索引数组 (16个)
         * @param useAlt 是否使用Alt作为读取源
         *
         * 教学要点:
         * 1. useAlt=true: readIndices指向Alt, writeIndices指向Main
         * 2. useAlt=false: readIndices指向Main, writeIndices指向Alt
         * 3. 自动设置脏标记
         */
        void FlipRenderTargets(const uint32_t mainIndices[16],
                               const uint32_t altIndices[16],
                               bool           useAlt);

        // ========================================================================
        // 批量同步接口
        // ========================================================================

        /**
         * @brief 同步所有脏数据到GPU (每帧必须调用)
         * @return 成功返回true
         *
         * 教学要点:
         * 1. 调用所有注册的Supplier获取最新值
         * 2. 更新CPU端9个结构体
         * 3. 检查9个脏标记,只上传被修改的Buffer
         * 4. 性能优化: 避免不必要的GPU上传
         */
        bool SyncToGPU();

        // ========================================================================
        // 数据访问接口
        // ========================================================================

        /**
         * @brief 获取Root Constants数据指针 (用于SetGraphicsRoot32BitConstants)
         * @return 指向RootConstants的指针
         */
        const RootConstants* GetRootConstants() const { return &m_rootConstants; }

        /**
         * @brief 获取Root Constants大小 (应始终为44)
         * @return 数据大小 (字节)
         */
        size_t GetRootConstantsSize() const { return sizeof(RootConstants); }

        /**
         * @brief 获取指定类别的Bindless索引
         */
        uint32_t GetCameraAndPlayerBufferIndex() const;
        uint32_t GetPlayerStatusBufferIndex() const;
        uint32_t GetScreenAndSystemBufferIndex() const;
        uint32_t GetIDBufferIndex() const;
        uint32_t GetWorldAndWeatherBufferIndex() const;
        uint32_t GetBiomeAndDimensionBufferIndex() const;
        uint32_t GetRenderingBufferIndex() const;
        uint32_t GetMatricesBufferIndex() const;
        uint32_t GetRenderTargetsBufferIndex() const;

        /**
         * @brief 重置为默认值
         *
         * 教学要点: 用于场景切换或管线重载
         */
        void Reset();

    private:
        // ========================================================================
        // 内部类型定义
        // ========================================================================

        /**
         * @brief 字段信息 (unordered_map值类型)
         *
         * 教学要点:
         * 1. categoryIndex: 0-8 (9个类别)
         * 2. offset: 字段在结构体中的偏移量 (offsetof)
         * 3. size: 字段大小 (sizeof)
         */
        struct FieldInfo
        {
            uint8_t  categoryIndex; // 0-8 (9个类别)
            uint16_t offset; // 字段偏移量
            uint16_t size; // 字段大小
        };

        /**
         * @brief Uniform Getter封装
         *
         * 教学要点:
         * 1. categoryIndex: 指定更新哪个类别的Buffer
         * 2. offset: 字段在结构体中的偏移量
         * 3. getter: std::function lambda,懒加载执行
         */
        struct UniformGetter
        {
            uint8_t                    categoryIndex; // 0-8
            uint16_t                   offset; // 字段偏移量
            std::function<void(void*)> getter; // 通用getter,写入目标地址
        };

        // ========================================================================
        // 内部辅助方法
        // ========================================================================

        /**
         * @brief 构建字段映射表 (初始化时调用一次)
         *
         * 教学要点:
         * 1. 使用offsetof宏计算字段偏移量
         * 2. 存储到unordered_map<string, FieldInfo>
         * 3. 只构建一次,后续O(1)查找
         */
        void BuildFieldMap();

        /**
         * @brief 注册通用Getter
         * @param categoryIndex 类别索引 (0-8)
         * @param offset 字段偏移量
         * @param getter 通用getter lambda
         */
        void RegisterGetter(uint8_t                    categoryIndex, uint16_t offset,
                            std::function<void(void*)> getter);

        /**
         * @brief 获取指定类别的CPU端数据指针
         * @param categoryIndex 类别索引 (0-8)
         * @return 指向结构体的void*指针
         */
        void* GetCategoryDataPtr(uint8_t categoryIndex);

        /**
         * @brief 标记指定类别为脏
         * @param categoryIndex 类别索引 (0-8)
         */
        void MarkCategoryDirty(uint8_t categoryIndex);

        /**
         * @brief 上传所有脏Buffer到GPU
         * @return 成功返回true
         */
        bool UploadDirtyBuffersToGPU();

        // ========================================================================
        // 成员变量
        // ========================================================================

        // CPU端数据 (9个结构体)
        RootConstants             m_rootConstants; // 44 bytes
        CameraAndPlayerUniforms   m_cameraAndPlayerUniforms; // ~112 bytes
        PlayerStatusUniforms      m_playerStatusUniforms; // ~80 bytes
        ScreenAndSystemUniforms   m_screenAndSystemUniforms; // ~96 bytes
        IDUniforms                m_idUniforms; // ~64 bytes
        WorldAndWeatherUniforms   m_worldAndWeatherUniforms; // ~176 bytes
        BiomeAndDimensionUniforms m_biomeAndDimensionUniforms; // ~160 bytes
        RenderingUniforms         m_renderingUniforms; // ~176 bytes
        MatricesUniforms          m_matricesUniforms; // 1152 bytes
        RenderTargetsIndexBuffer  m_renderTargetsIndexBuffer; // 128 bytes

        // GPU资源 (9个StructuredBuffer)
        class D12Buffer* m_cameraAndPlayerBuffer; // GPU端CameraAndPlayerUniforms
        class D12Buffer* m_playerStatusBuffer; // GPU端PlayerStatusUniforms
        class D12Buffer* m_screenAndSystemBuffer; // GPU端ScreenAndSystemUniforms
        class D12Buffer* m_idBuffer; // GPU端IDUniforms
        class D12Buffer* m_worldAndWeatherBuffer; // GPU端WorldAndWeatherUniforms
        class D12Buffer* m_biomeAndDimensionBuffer; // GPU端BiomeAndDimensionUniforms
        class D12Buffer* m_renderingBuffer; // GPU端RenderingUniforms
        class D12Buffer* m_matricesBuffer; // GPU端MatricesUniforms
        class D12Buffer* m_renderTargetsBuffer; // GPU端RenderTargetsBuffer

        // 脏标记 (9个)
        bool m_cameraAndPlayerDirty;
        bool m_playerStatusDirty;
        bool m_screenAndSystemDirty;
        bool m_idDirty;
        bool m_worldAndWeatherDirty;
        bool m_biomeAndDimensionDirty;
        bool m_renderingDirty;
        bool m_matricesDirty;
        bool m_renderTargetsDirty;

        // 字段映射表 (unordered_map, 只构建一次)
        std::unordered_map<std::string, FieldInfo> m_fieldMap;

        // Supplier列表 (所有注册的Getter)
        std::vector<UniformGetter> m_uniformGetters;

        // 禁用拷贝 (遵循RAII原则)
        UniformManager(const UniformManager&)            = delete;
        UniformManager& operator=(const UniformManager&) = delete;
    };
} // namespace enigma::graphic
