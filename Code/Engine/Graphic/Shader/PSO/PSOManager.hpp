/**
 * @file PSOManager.hpp
 * @brief PSO动态管理器 - 基于渲染状态的PSO缓存系统
 * @date 2025-11-01
 *
 * 职责:
 * 1. 动态创建和缓存PSO (基于ShaderProgram + RenderState)
 * 2. 解决RT格式硬编码问题 (支持运行时RT格式)
 * 3. 集成SetBlendMode/SetDepthMode状态管理
 * 4. 提供GetOrCreatePSO统一接口
 *
 * 设计决策:
 * - PSOKey使用ShaderProgram指针 (简化设计，避免Hash冲突)
 * - 使用std::unordered_map缓存PSO (O(1)查找)
 * - 16字节对齐优化 (Cache-Friendly)
 */

#pragma once

#include "Engine/Graphic/Core/RenderState.hpp"
#include <d3d12.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <cstddef>

namespace enigma::graphic
{
    class ShaderProgram;
#pragma warning(push)
#pragma warning(disable: 4324)
    /**
     * @struct PSOKey
     * @brief PSO缓存键 - 唯一标识一个PSO配置
     *
     * 教学要点:
     * - 使用ShaderProgram指针而非Hash (简化设计)
     * - 包含所有影响PSO的状态 (RT格式、混合、深度)
     * - 16字节对齐优化Cache性能
     *
     * 架构对应:
     * ```
     * Iris                    DirectX 12
     * ----------------------------------------
     * Program + State    →    PSOKey
     * glUseProgram()     →    GetOrCreatePSO()
     * ```
     */
    struct alignas(16) PSOKey
    {
        const ShaderProgram* shaderProgram       = nullptr; ///< 着色器程序指针
        DXGI_FORMAT          rtFormats[8]        = {}; ///< 8个RT格式 (对应colortex0-7)
        DXGI_FORMAT          depthFormat         = DXGI_FORMAT_UNKNOWN; ///< 深度格式
        BlendMode            blendMode           = BlendMode::Opaque; ///< 混合模式
        DepthMode            depthMode           = DepthMode::Enabled; ///< 深度模式
        StencilTestDetail    stencilDetail       = StencilTestDetail::Disabled(); ///< Stencil test configuration
        RasterizationConfig  rasterizationConfig = RasterizationConfig::Default(); ///< Rasterization configuration

        /**
         * @brief 相等比较运算符
         * @param other 另一个PSOKey
         * @return 相等返回true
         */
        bool operator==(const PSOKey& other) const;

        /**
         * @brief 计算Hash值
         * @return Hash值
         *
         * 教学要点:
         * - 使用std::hash组合多个字段
         * - ShaderProgram指针直接Hash (避免重复计算)
         */
        std::size_t GetHash() const;
    };

    /**
     * @struct PSOKeyHash
     * @brief PSOKey的Hash函数对象
     *
     * 教学要点:
     * - 用于std::unordered_map的Hash函数
     * - 调用PSOKey::GetHash()实现
     */
    struct PSOKeyHash
    {
        std::size_t operator()(const PSOKey& key) const
        {
            return key.GetHash();
        }
    };

    /**
     * @class PSOManager
     * @brief PSO动态管理器 - 统一管理PSO创建和缓存
     *
     * 教学要点:
     * 1. 解决RT格式硬编码问题 (运行时动态创建PSO)
     * 2. 集成SetBlendMode/SetDepthMode (状态驱动PSO)
     * 3. 缓存机制避免重复创建 (性能优化)
     * 4. 符合Iris架构 (Program + State → PSO)
     *
     * 使用示例:
     * ```cpp
     * // 1. 设置渲染状态
     * SetBlendMode(BlendMode::Alpha);
     * SetDepthMode(DepthMode::ReadOnly);
     *
     * // 2. 获取或创建PSO
     * auto* pso = psoManager.GetOrCreatePSO(
     *     shaderProgram,
     *     rtFormats,
     *     depthFormat
     * );
     *
     * // 3. 使用PSO
     * commandList->SetPipelineState(pso);
     * ```
     */
    class PSOManager
    {
    public:
        PSOManager()  = default;
        ~PSOManager() = default;

        // 禁用拷贝
        PSOManager(const PSOManager&)            = delete;
        PSOManager& operator=(const PSOManager&) = delete;

        // 支持移动
        PSOManager(PSOManager&&) noexcept            = default;
        PSOManager& operator=(PSOManager&&) noexcept = default;

        /**
         * @brief 获取或创建PSO
         * @param shaderProgram 着色器程序
         * @param rtFormats RT格式数组 (8个)
         * @param depthFormat 深度格式
         * @param blendMode 混合模式
         * @param depthMode 深度模式
         * @return PSO指针 (失败返回nullptr)
         *
         * 教学要点:
         * 1. 构造PSOKey查找缓存
         * 2. 缓存命中直接返回
         * 3. 缓存未命中调用CreatePSO创建
         * 4. 新PSO加入缓存
         */
        ID3D12PipelineState* GetOrCreatePSO(
            const ShaderProgram*       shaderProgram,
            const DXGI_FORMAT          rtFormats[8],
            DXGI_FORMAT                depthFormat,
            BlendMode                  blendMode,
            DepthMode                  depthMode,
            const StencilTestDetail&   stencilDetail,
            const RasterizationConfig& rasterizationConfig
        );

        /**
         * @brief 清空PSO缓存
         *
         * 教学要点:
         * - 释放所有缓存的PSO
         * - 用于热重载或资源清理
         */
        void ClearCache();

    private:
        /**
         * @brief 创建新的PSO
         * @param key PSO键
         * @return PSO ComPtr (失败返回nullptr)
         *
         * 教学要点:
         * 1. 根据PSOKey配置PSO描述符
         * 2. 设置RT格式 (从key.rtFormats)
         * 3. 设置混合模式 (从key.blendMode)
         * 4. 设置深度模式 (从key.depthMode)
         * 5. 调用D3D12Device::CreateGraphicsPipelineState
         *
         * 注意: 需要ShaderProgram提供着色器字节码访问方法
         */
        Microsoft::WRL::ComPtr<ID3D12PipelineState> CreatePSO(const PSOKey& key);

        /**
         * @brief 配置混合状态
         * @param blendDesc 混合描述符
         * @param blendMode 混合模式
         */
        static void ConfigureBlendState(D3D12_BLEND_DESC& blendDesc, BlendMode blendMode);

        /**
         * @brief 配置深度模板状态
         * @param depthStencilDesc 深度模板描述符
         * @param depthMode 深度模式
         * @param stencilDetail Stencil test configuration
         */
        static void ConfigureDepthStencilState(
            D3D12_DEPTH_STENCIL_DESC& depthStencilDesc,
            DepthMode                 depthMode,
            const StencilTestDetail&  stencilDetail
        );

        /**
         * @brief 配置光栅化状态
         * @param rasterDesc 光栅化描述符
         * @param config Rasterization configuration
         */
        static void ConfigureRasterizerState(D3D12_RASTERIZER_DESC& rasterDesc, const RasterizationConfig& config);

    private:
        /// PSO缓存 (Key → PSO)
        std::unordered_map<PSOKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>, PSOKeyHash> m_psoCache;
    };
} // namespace enigma::graphic
