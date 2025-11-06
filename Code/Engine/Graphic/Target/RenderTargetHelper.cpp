/**
 * @file RenderTargetHelper.cpp
 * @brief RenderTarget系统辅助工具类实现
 * @date 2025-11-05
 * @author Caizii
 *
 * 实现说明:
 * 本文件实现RenderTargetHelper的三个核心静态方法：
 * 1. CalculateRTMemoryUsage - 计算RT内存使用量
 * 2. ValidateRTConfiguration - 验证RT配置有效性
 * 3. GenerateRTConfigs - 生成RT配置数组
 *
 * 教学要点:
 * - 工具类实现模式：纯静态方法，无状态管理
 * - 内存计算最佳实践：考虑格式字节数、双缓冲机制
 * - 配置验证模式：早期错误检测，详细错误信息
 * - 默认配置策略：提供合理的默认值
 */

#include "Engine/Graphic/Target/RenderTargetHelper.hpp"
#include "Engine/Core/EngineCommon.hpp"

namespace enigma::graphic
{
    // ============================================================================
    // 内存计算函数实现
    // ============================================================================

    /**
     * @brief 根据DXGI_FORMAT获取每像素字节数
     * @param format DXGI格式
     * @return size_t 每像素字节数
     *
     * 教学要点:
     * - 常见格式的字节数映射
     * - 默认值处理（未知格式返回4字节）
     * - 可扩展设计（添加新格式只需添加case）
     */
    static size_t GetBytesPerPixel(DXGI_FORMAT format)
    {
        switch (format)
        {
        // 8位格式 (1字节/像素)
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return 1;

        // 16位格式 (2字节/像素)
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
            return 2;

        // 32位格式 (4字节/像素)
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            return 4;

        // 64位格式 (8字节/像素)
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
            return 8;

        // 128位格式 (16字节/像素)
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 16;

        // 默认值：未知格式返回4字节（最常见的RGBA8格式）
        default:
            DebuggerPrintf(
                "RenderTargetHelper::GetBytesPerPixel - Unknown format %d, defaulting to 4 bytes/pixel\n",
                static_cast<int>(format)
            );
            return 4;
        }
    }

    size_t RenderTargetHelper::CalculateRTMemoryUsage(
        int         width,
        int         height,
        int         colorTexCount,
        DXGI_FORMAT format
    )
    {
        // 参数验证
        if (width <= 0 || height <= 0 || colorTexCount <= 0)
        {
            DebuggerPrintf(
                "RenderTargetHelper::CalculateRTMemoryUsage - Invalid parameters: width=%d, height=%d, colorTexCount=%d\n",
                width, height, colorTexCount
            );
            return 0;
        }

        // 获取每像素字节数
        size_t bytesPerPixel = GetBytesPerPixel(format);

        // 计算单个纹理内存：width * height * bytesPerPixel
        size_t singleTextureMemory = static_cast<size_t>(width) * static_cast<size_t>(height) * bytesPerPixel;

        // 计算单个RT内存：singleTextureMemory * 2 (Main + Alt双缓冲)
        size_t singleRTMemory = singleTextureMemory * 2;

        // 计算总内存：singleRTMemory * colorTexCount
        size_t totalMemory = singleRTMemory * static_cast<size_t>(colorTexCount);

        // 调试日志
        DebuggerPrintf(
            "RenderTargetHelper::CalculateRTMemoryUsage - Resolution: %dx%d, Format: %d (%zu bytes/pixel), ColorTexCount: %d\n",
            width, height, static_cast<int>(format), bytesPerPixel, colorTexCount
        );
        DebuggerPrintf(
            "  Single Texture: %zu bytes (%.2f MB)\n",
            singleTextureMemory, singleTextureMemory / (1024.0 * 1024.0)
        );
        DebuggerPrintf(
            "  Single RT (Main+Alt): %zu bytes (%.2f MB)\n",
            singleRTMemory, singleRTMemory / (1024.0 * 1024.0)
        );
        DebuggerPrintf(
            "  Total Memory: %zu bytes (%.2f MB)\n",
            totalMemory, totalMemory / (1024.0 * 1024.0)
        );

        return totalMemory;
    }

    // ============================================================================
    // 配置验证函数实现
    // ============================================================================

    RenderTargetHelper::RTValidationResult RenderTargetHelper::ValidateRTConfiguration(
        int colorTexCount,
        int maxColorTextures
    )
    {
        // 验证maxColorTextures有效性
        if (maxColorTextures <= 0)
        {
            return RTValidationResult::Invalid(
                Stringf("maxColorTextures (%d) must be greater than 0", maxColorTextures)
            );
        }

        // 验证colorTexCount最小值
        if (colorTexCount < 1)
        {
            return RTValidationResult::Invalid(
                Stringf("colorTexCount (%d) is less than minimum required (1)", colorTexCount)
            );
        }

        // 验证colorTexCount最大值
        if (colorTexCount > maxColorTextures)
        {
            return RTValidationResult::Invalid(
                Stringf("colorTexCount (%d) exceeds maxColorTextures (%d)", colorTexCount, maxColorTextures)
            );
        }

        // 配置有效
        DebuggerPrintf(
            "RenderTargetHelper::ValidateRTConfiguration - Configuration is valid: colorTexCount=%d, maxColorTextures=%d\n",
            colorTexCount, maxColorTextures
        );
        return RTValidationResult::Valid();
    }

    // ============================================================================
    // 配置生成函数实现
    // ============================================================================

    std::array<RTConfig, 16> RenderTargetHelper::GenerateRTConfigs(int colorTexCount)
    {
        // 修正colorTexCount范围到[1, 16]
        int actualColorTexCount = colorTexCount;
        if (actualColorTexCount < 1)
        {
            DebuggerPrintf(
                "RenderTargetHelper::GenerateRTConfigs - colorTexCount (%d) is less than 1, correcting to 1\n",
                colorTexCount
            );
            actualColorTexCount = 1;
        }
        else if (actualColorTexCount > 16)
        {
            DebuggerPrintf(
                "RenderTargetHelper::GenerateRTConfigs - colorTexCount (%d) exceeds 16, correcting to 16\n",
                colorTexCount
            );
            actualColorTexCount = 16;
        }

        // 创建配置数组
        std::array<RTConfig, 16> configs;

        // 生成有效的RT配置（前actualColorTexCount个）
        for (int i = 0; i < actualColorTexCount; ++i)
        {
            // 生成名称：colortex0, colortex1, ..., colortex15
            std::string name = Stringf("colortex%d", i);

            // 创建配置
            configs[i] = RTConfig::ColorTarget(
                name,
                0, 0, // 尺寸设为0，由RenderTargetManager根据baseWidth/baseHeight计算
                DXGI_FORMAT_R8G8B8A8_UNORM, // 默认格式
                true, // 启用Flipper（支持历史帧访问）
                LoadAction::Clear, // 每帧清空
                ClearValue::Color(Rgba8::BLACK), // 清空为黑色
                false, // 不启用Mipmap
                true, // 允许线性过滤
                1 // 无MSAA
            );
        }

        // 生成占位配置（剩余的槽位）
        for (int i = actualColorTexCount; i < 16; ++i)
        {
            // 生成占位名称：unused_colortex4, unused_colortex5, ...
            std::string name = Stringf("unused_colortex%d", i);

            // 创建占位配置
            configs[i] = RTConfig::ColorTarget(
                name,
                1, 1, // 最小尺寸
                DXGI_FORMAT_R8G8B8A8_UNORM, // 默认格式
                false, // 禁用Flipper
                LoadAction::DontCare, // 不关心内容
                ClearValue::Color(Rgba8::BLACK), // 清空为黑色（虽然不会被使用）
                false, // 不启用Mipmap
                true, // 允许线性过滤
                1 // 无MSAA
            );
        }

        // 调试日志
        DebuggerPrintf(
            "RenderTargetHelper::GenerateRTConfigs - Generated %d active RT configs + %d placeholder configs\n",
            actualColorTexCount, 16 - actualColorTexCount
        );

        return configs;
    }
} // namespace enigma::graphic
