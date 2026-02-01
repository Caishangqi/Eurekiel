#pragma once

#include <cstdint>
#include <string>
#include <dxgi.h>
#include "Engine/Core/Rgba8.hpp"

namespace enigma::graphic
{
    // ============================================================================  
    // RenderTargetType - 渲染目标类型枚举
    // ============================================================================

    /**
     * @brief 渲染目标类型分类
     * 
     * 对应Iris延迟渲染管线的四类渲染目标:
     * - ColorTex: 常规颜色渲染目标 (0-15)
     * - ShadowColor: 阴影颜色渲染目标 (0-7) 
     * - DepthTex: 深度渲染目标 (0-2)
     * - ShadowTex: 阴影贴图 (0-1)
     * 
     * 教学要点: 使用enum class确保类型安全，避免隐式转换
     */
    enum class RenderTargetType
    {
        ColorTex, ///< 常规颜色渲染目标，使用D12RenderTarget
        ShadowColor, ///< 阴影颜色渲染目标，使用D12RenderTarget，支持Flipper
        DepthTex, ///< 深度渲染目标，使用D12DepthTexture  
        ShadowTex ///< 阴影贴图，使用D12Texture，只读无Flipper
    };

    // ============================================================================  
    // LoadAction - 装载动作枚举
    // ============================================================================

    /**
     * @brief 渲染目标绑定时的装载动作
     * 
     * 对应D3D12_RENDER_TARGET_BEGINNING_ACCESS_TYPE的设计理念
     * - Load: 保留现有内容 (适用于需要历史帧数据的场景)
     * - Clear: 清空到指定值 (适用于每帧全新开始的场景)
     * - DontCare: 不关心内容 (性能最优，适用于内容完全覆盖的场景)
     * 
     * 教学要点: 遵循D3D12最佳实践，明确资源状态管理
     */
    enum class LoadAction
    {
        Load, ///< 保留现有内容
        Clear, ///< 清空到指定值
        DontCare ///< 不关心内容 (性能最优)
    };

    // ============================================================================  
    // ClearValue - 清空值联合体  
    // ============================================================================

    /**
     * @brief 渲染目标清空值
     * 
     * 设计灵感来自D3D12_CLEAR_VALUE，使用union节省内存
     * 支持颜色和深度两种清空方式
     * 
     * 教学要点:
     * - union确保同一时间只存储一种类型的值
     * - 使用项目已有的Rgba8结构表示颜色
     * - 静态工厂方法提供类型安全的创建方式
     */
    struct ClearValue
    {
        union
        {
            Rgba8 colorRgba8; ///< 颜色清空值 (使用Rgba8)
            struct DepthStencil
            {
                float   depth; ///< 深度清空值
                uint8_t stencil; ///< 模板清空值
            } depthStencil; ///< 深度模板清空值
        };

        // 默认构造函数：初始化为黑色
        ClearValue() : colorRgba8(Rgba8::BLACK)
        {
        }

        // 拷贝构造函数：显式定义以支持union成员的拷贝
        ClearValue(const ClearValue& other) : colorRgba8(other.colorRgba8)
        {
        }

        // 拷贝赋值运算符：显式定义以支持union成员的赋值
        ClearValue& operator=(const ClearValue& other)
        {
            if (this != &other)
            {
                colorRgba8 = other.colorRgba8;
            }
            return *this;
        }

        /**
         * @brief 创建颜色清空值（使用Rgba8）
         * @param rgba8 Rgba8颜色值
         * @return ClearValue实例
         * 
         * 教学要点: 使用项目已有Rgba8结构，避免重复造轮子
         */
        static ClearValue Color(const Rgba8& rgba8);

        /**
         * @brief 创建颜色清空值（使用浮点分量）
         * @param r 红色分量 [0-1]
         * @param g 绿色分量 [0-1] 
         * @param b 蓝色分量 [0-1]
         * @param a 透明度分量 [0-1]
         * @return ClearValue实例
         * 
         * 教学要点: 便捷方法，内部转换为Rgba8
         */
        static ClearValue Color(float r, float g, float b, float a = 1.0f);

        /// The clear value of 3 channel format like RGB
        /// @param r R channel clear value 
        /// @param g G channel clear value
        /// @param b B channel clear value
        /// @return The Clear value union contains RGB value
        [[deprecated]] static ClearValue Color(float r, float g, float b);

        /**
         * @brief 创建深度模板清空值
         * @param depth 深度值 [0-1]
         * @param stencil 模板值 [0-255]
         * @return ClearValue实例
         */
        static ClearValue Depth(float depth, uint8_t stencil = 0);

        /**
         * @brief 获取颜色的浮点数组表示（用于DirectX API调用）
         * @param outFloats 输出的4元素float数组
         * 
         * 教学要点: D3D12 API需要float数组，Rgba8提供GetAsFloats()转换
         */
        void GetColorAsFloats(float* outFloats) const;
    };

    // ============================================================================  
    // RenderTargetConfig - 渲染目标配置结构体
    // ============================================================================

    /**
     * @brief 渲染目标创建配置
     * 
     * 集中管理所有创建渲染目标所需的参数
     * 支持不同类型的渲染目标配置
     * 
     * 教学要点:
     * - 结构体聚合所有配置参数，便于传递和管理
     * - enableFlipper仅对需要历史帧数据的RT有效
     * - loadAction和clearValue配合实现高效的清空策略
     */
    struct RenderTargetConfig
    {
        std::string name          = ""; ///< 渲染目标名称 (用于调试)
        int         width         = 0; ///< 宽度 (像素), 0表示使用scale计算
        int         height        = 0; ///< 高度 (像素), 0表示使用scale计算
        DXGI_FORMAT format        = DXGI_FORMAT_R8G8B8A8_UNORM; ///< 像素格式
        bool        enableFlipper = true; ///< 是否启用Main/Alt翻转机制
        LoadAction  loadAction    = LoadAction::Clear; ///< 装载动作
        ClearValue  clearValue; ///< 清空值 (默认黑色)

        // ========================================================================
        // 纹理特性字段 (配置统一化重构完成)
        // ========================================================================
        bool enableMipmap      = false; ///< 是否启用Mipmap (默认关闭)
        bool allowLinearFilter = true; ///< 是否允许线性过滤 (默认开启)
        int  sampleCount       = 1; ///< MSAA采样数 (默认1=无MSAA)

        // ========================================================================
        // 相对缩放字段 - 支持动态分辨率调整 (配置统一化重构)
        // ========================================================================
        float widthScale  = 1.0f; ///< 宽度相对屏幕的缩放 (0.5 = 半分辨率)
        float heightScale = 1.0f; ///< 高度相对屏幕的缩放

        /**
         * @brief 创建颜色渲染目标配置的静态工厂方法
         * @param name 名称
         * @param width 宽度
         * @param height 高度
         * @param format 格式
         * @param enableFlipper 是否启用翻转
         * @param loadAction 装载动作
         * @param clearValue 清空值（默认黑色）
         * @param enableMipmap 是否启用Mipmap（默认关闭）
         * @param allowLinearFilter 是否允许线性过滤（默认开启）
         * @param sampleCount MSAA采样数（默认1=无MSAA）
         * @return RTConfig实例
         * 
         * 教学要点: 
         * - 默认使用Rgba8::BLACK清空值
         * - 新增纹理特性参数保持向后兼容（使用默认值）
         */
        static RenderTargetConfig ColorTarget(
            const std::string& name,
            int                width, int height,
            DXGI_FORMAT        format,
            bool               enableFlipper     = true,
            LoadAction         loadAction        = LoadAction::Clear,
            ClearValue         clearValue        = ClearValue::Color(Rgba8::BLACK),
            bool               enableMipmap      = false,
            bool               allowLinearFilter = true,
            int                sampleCount       = 1
        );

        /**
         * @brief 创建深度渲染目标配置的静态工厂方法
         * @param name 名称  
         * @param width 宽度
         * @param height 高度
         * @param format 格式
         * @param enableFlipper 是否启用翻转
         * @param loadAction 装载动作
         * @param clearValue 清空值（默认1.0深度+0模板）
         * @param enableMipmap 是否启用Mipmap（默认关闭）
         * @param allowLinearFilter 是否允许线性过滤（默认开启）
         * @param sampleCount MSAA采样数（默认1=无MSAA）
         * @return RTConfig实例
         * 
         * 教学要点: 新增纹理特性参数保持向后兼容（使用默认值）
         */
        static RenderTargetConfig DepthTarget(
            const std::string& name,
            int                width, int height,
            DXGI_FORMAT        format,
            bool               enableFlipper     = false,
            LoadAction         loadAction        = LoadAction::Clear,
            ClearValue         clearValue        = ClearValue::Depth(1.0f, 0),
            bool               enableMipmap      = false,
            bool               allowLinearFilter = true,
            int                sampleCount       = 1
        );

        /**
         * @brief 创建支持相对缩放的颜色渲染目标配置
         * @param name 名称
         * @param widthScale 宽度相对屏幕的缩放 (0.5 = 半分辨率)
         * @param heightScale 高度相对屏幕的缩放
         * @param format 格式
         * @param enableFlipper 是否启用翻转
         * @param loadAction 装载动作
         * @param clearValue 清空值（默认黑色）
         * @param enableMipmap 是否启用Mipmap（默认关闭）
         * @param allowLinearFilter 是否允许线性过滤（默认开启）
         * @param sampleCount MSAA采样数（默认1=无MSAA）
         * @return RTConfig实例
         *
         * 教学要点:
         * - 支持相对缩放配置，实际尺寸由RenderTargetManager构造时计算
         * - width和height暂时设为0，通过widthScale/heightScale字段传递缩放信息
         * - 统一配置接口，RTConfig作为唯一配置源(配置统一化重构已完成)
         */
        static RenderTargetConfig ColorTargetWithScale(
            const std::string& name,
            float              widthScale,
            float              heightScale,
            DXGI_FORMAT        format,
            bool               enableFlipper     = true,
            LoadAction         loadAction        = LoadAction::Clear,
            ClearValue         clearValue        = ClearValue::Color(Rgba8::BLACK),
            bool               enableMipmap      = false,
            bool               allowLinearFilter = true,
            int                sampleCount       = 1
        );

        /**
         * @brief 创建支持相对缩放的深度渲染目标配置
         * @param name 名称
         * @param widthScale 宽度相对屏幕的缩放 (0.5 = 半分辨率)
         * @param heightScale 高度相对屏幕的缩放
         * @param format 格式
         * @param loadAction 装载动作
         * @param clearValue 清空值（默认1.0深度+0模板）
         * @param sampleCount MSAA采样数（默认1=无MSAA）
         * @return RTConfig实例
         *
         * 教学要点:
         * - 深度纹理版本的相对缩放配置
         * - 深度纹理通常不启用Flipper（enableFlipper固定为false）
         * - 深度纹理通常不启用Mipmap（enableMipmap固定为false）
         * - width和height暂时设为0，通过widthScale/heightScale字段传递缩放信息
         */
        static RenderTargetConfig DepthTargetWithScale(
            const std::string& name,
            float              widthScale,
            float              heightScale,
            DXGI_FORMAT        format,
            LoadAction         loadAction  = LoadAction::Clear,
            ClearValue         clearValue  = ClearValue::Depth(1.0f, 0),
            int                sampleCount = 1
        );

        // ========================================================================
        // [NEW] Shadow Render Target Factory Methods
        // Shadow textures use square resolution (width = height = resolution)
        // ========================================================================

        /**
         * @brief Create shadow color render target config
         * @param name Target name for debugging
         * @param resolution Square resolution (width = height)
         * @param format Pixel format (default: R8G8B8A8_UNORM)
         * @param enableFlipper Enable Main/Alt flip mechanism
         * @param loadAction Load action
         * @param clearValue Clear value (default: black)
         * @return RTConfig instance
         */
        static RenderTargetConfig ShadowColorTarget(
            const std::string& name,
            int                resolution,
            DXGI_FORMAT        format        = DXGI_FORMAT_R8G8B8A8_UNORM,
            bool               enableFlipper = true,
            LoadAction         loadAction    = LoadAction::Clear,
            ClearValue         clearValue    = ClearValue::Color(Rgba8::BLACK)
        );

        /**
         * @brief Create shadow depth render target config
         * @param name Target name for debugging
         * @param resolution Square resolution (width = height)
         * @param format Pixel format (default: D32_FLOAT)
         * @param loadAction Load action
         * @param clearValue Clear value (default: 1.0 depth + 0 stencil)
         * @return RTConfig instance
         */
        static RenderTargetConfig ShadowDepthTarget(
            const std::string& name,
            int                resolution,
            DXGI_FORMAT        format     = DXGI_FORMAT_D32_FLOAT,
            LoadAction         loadAction = LoadAction::Clear,
            ClearValue         clearValue = ClearValue::Depth(1.0f, 0)
        );
    };
} // namespace enigma::graphic
