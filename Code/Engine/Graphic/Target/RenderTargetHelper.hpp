/**
 * @file RenderTargetHelper.hpp
 * @brief RenderTarget系统辅助工具类 - 统一管理RT相关工具函数
 * @date 2025-11-05
 * @author Caizii
 *
 * 架构说明:
 * RenderTargetHelper 是一个纯静态工具类，为RenderTarget系统提供便捷的工具函数接口。
 * 它整合了内存计算、配置验证和配置生成等常用功能，简化RenderTarget系统的使用。
 *
 * 设计原则:
 * - 纯静态类: 所有函数都是static，禁止实例化
 * - 统一接口: 为RenderTarget系统提供一致的工具函数访问方式
 * - 便捷封装: 简化常用操作的实现复杂度
 * - 功能分组: 按功能域组织函数（内存计算、配置验证、配置生成）
 *
 * 职责边界:
 * - + RT内存使用计算 (CalculateRTMemoryUsage)
 * - + RT配置验证 (ValidateRTConfiguration)
 * - + RT配置生成 (GenerateRTConfigs)
 * - ❌ 不负责具体的RT创建实现 (委托给D12RenderTarget)
 * - ❌ 不负责RT生命周期管理 (委托给RenderTargetManager)
 * - ❌ 不负责GPU资源操作 (委托给D3D12 API)
 *
 * 使用示例:
 * @code
 * // ========== 内存计算示例 ==========
 * 
 * // 1. 计算单个RT的内存使用
 * size_t singleRTMemory = RenderTargetHelper::CalculateRTMemoryUsage(
 *     1920, 1080, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
 * // singleRTMemory = 8,294,400 bytes (~8.3MB)
 * 
 * // 2. 计算16个RT的总内存使用
 * size_t totalMemory = RenderTargetHelper::CalculateRTMemoryUsage(
 *     1920, 1080, 16, DXGI_FORMAT_R8G8B8A8_UNORM);
 * // totalMemory = 132,710,400 bytes (~132.7MB)
 * 
 * // 3. 计算4个RT的内存使用（内存优化）
 * size_t optimizedMemory = RenderTargetHelper::CalculateRTMemoryUsage(
 *     1920, 1080, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
 * // optimizedMemory = 33,177,600 bytes (~33.2MB)
 * // 节省: 132.7MB - 33.2MB = 99.5MB (75%)
 * 
 * // ========== 配置验证示例 ==========
 * 
 * // 4. 验证有效的RT配置
 * auto result1 = RenderTargetHelper::ValidateRTConfiguration(8, 16);
 * if (result1.isValid) {
 *     std::cout << "Configuration is valid" << std::endl;
 * }
 * 
 * // 5. 验证无效的RT配置（超出范围）
 * auto result2 = RenderTargetHelper::ValidateRTConfiguration(20, 16);
 * if (!result2.isValid) {
 *     std::cerr << "Error: " << result2.errorMessage << std::endl;
 *     // 输出: "Error: colorTexCount (20) exceeds maxColorTextures (16)"
 * }
 * 
 * // 6. 验证无效的RT配置（小于最小值）
 * auto result3 = RenderTargetHelper::ValidateRTConfiguration(0, 16);
 * if (!result3.isValid) {
 *     std::cerr << "Error: " << result3.errorMessage << std::endl;
 *     // 输出: "Error: colorTexCount (0) is less than minimum required (1)"
 * }
 * 
 * // ========== 配置生成示例 ==========
 * 
 * // 7. 生成16个RT的默认配置
 * auto configs16 = RenderTargetHelper::GenerateRTConfigs(16);
 * // configs16[0] = colortex0 (1920x1080, R8G8B8A8_UNORM, Flipper enabled)
 * // configs16[1] = colortex1 (1920x1080, R8G8B8A8_UNORM, Flipper enabled)
 * // ...
 * // configs16[15] = colortex15 (1920x1080, R8G8B8A8_UNORM, Flipper enabled)
 * 
 * // 8. 生成4个RT的优化配置
 * auto configs4 = RenderTargetHelper::GenerateRTConfigs(4);
 * // configs4[0-3] = 有效配置
 * // configs4[4-15] = 占位配置（不会被使用）
 * 
 * // ========== 完整使用流程示例 ==========
 * 
 * // 完整流程：验证配置 -> 生成配置 -> 计算内存 -> 创建RenderTargetManager
 * int colorTexCount = 8;
 * int maxColorTextures = 16;
 * 
 * // 1. 验证配置
 * auto validation = RenderTargetHelper::ValidateRTConfiguration(colorTexCount, maxColorTextures);
 * if (!validation.isValid) {
 *     std::cerr << "Configuration error: " << validation.errorMessage << std::endl;
 *     return;
 * }
 * 
 * // 2. 生成配置
 * auto rtConfigs = RenderTargetHelper::GenerateRTConfigs(colorTexCount);
 * 
 * // 3. 计算内存使用
 * size_t memoryUsage = RenderTargetHelper::CalculateRTMemoryUsage(
 *     1920, 1080, colorTexCount, DXGI_FORMAT_R8G8B8A8_UNORM);
 * std::cout << "Estimated memory usage: " << (memoryUsage / 1024 / 1024) << " MB" << std::endl;
 * 
 * // 4. 创建RenderTargetManager
 * auto rtManager = std::make_unique<RenderTargetManager>(
 *     1920, 1080, rtConfigs, colorTexCount);
 * @endcode
 *
 * 教学要点:
 * - 工具类模式 (Utility Class Pattern)
 * - 静态方法设计 (Static Method Design)
 * - 便捷函数封装 (Convenience Function Wrapper)
 * - 内存计算最佳实践 (Memory Calculation Best Practices)
 * - 配置验证模式 (Configuration Validation Pattern)
 */

#pragma once

#include "Engine/Graphic/Target/RTTypes.hpp"
#include <array>
#include <string>
#include <cstddef>
#include <dxgi.h>

namespace enigma::graphic
{
    /**
     * @class RenderTargetHelper
     * @brief RenderTarget系统辅助工具类 - 纯静态工具类
     *
     * 设计理念:
     * - 纯静态类：所有方法都是static，禁止实例化
     * - 功能分组：按职责域组织（内存计算、配置验证、配置生成）
     * - 便捷封装：简化RenderTarget系统的常用操作
     * - 统一接口：为RenderTarget系统提供一致的工具函数访问方式
     *
     * 功能分组:
     * 1. **内存计算函数**：
     *    - CalculateRTMemoryUsage: 计算RT内存使用量
     *
     * 2. **配置验证函数**：
     *    - ValidateRTConfiguration: 验证RT配置有效性
     *
     * 3. **配置生成函数**：
     *    - GenerateRTConfigs: 生成RT配置数组
     *
     * 教学要点:
     * - 工具类模式 (Utility Class Pattern)
     * - 静态方法设计 (Static Method Design)
     * - 配置验证模式 (Configuration Validation Pattern)
     * - 内存计算最佳实践 (Memory Calculation Best Practices)
     */
    class RenderTargetHelper
    {
    public:
        // 禁止实例化 (纯静态工具类)
        RenderTargetHelper()                                     = delete;
        ~RenderTargetHelper()                                    = delete;
        RenderTargetHelper(const RenderTargetHelper&)            = delete;
        RenderTargetHelper& operator=(const RenderTargetHelper&) = delete;

        // ========================================================================
        // 内存计算函数组
        // ========================================================================

        /**
         * @brief 计算RenderTarget的内存使用量
         * @param width RT宽度（像素）
         * @param height RT高度（像素）
         * @param colorTexCount 颜色纹理数量（1-16）
         * @param format 像素格式
         * @return size_t 总内存使用量（字节）
         *
         * 教学要点:
         * - 内存计算公式：width * height * bytesPerPixel * colorTexCount * 2 (Main + Alt)
         * - 格式字节数映射：R8G8B8A8 = 4字节，R16G16B16A16_FLOAT = 8字节等
         * - 考虑Main/Alt双缓冲机制（乘以2）
         * - 用于内存预算和性能优化决策
         *
         * 计算公式:
         * ```
         * 单个纹理内存 = width * height * bytesPerPixel
         * 单个RT内存 = 单个纹理内存 * 2 (Main + Alt)
         * 总内存 = 单个RT内存 * colorTexCount
         * ```
         *
         * 示例:
         * @code
         * // 计算1920x1080分辨率，16个R8G8B8A8格式RT的内存使用
         * size_t memory = RenderTargetHelper::CalculateRTMemoryUsage(
         *     1920, 1080, 16, DXGI_FORMAT_R8G8B8A8_UNORM);
         * // memory = 1920 * 1080 * 4 * 16 * 2 = 265,420,800 bytes (~265.4MB)
         *
         * // 计算4个RT的内存使用（内存优化）
         * size_t optimized = RenderTargetHelper::CalculateRTMemoryUsage(
         *     1920, 1080, 4, DXGI_FORMAT_R8G8B8A8_UNORM);
         * // optimized = 1920 * 1080 * 4 * 4 * 2 = 66,355,200 bytes (~66.4MB)
         * // 节省: 265.4MB - 66.4MB = 199MB (75%)
         * @endcode
         *
         * 支持的格式:
         * - DXGI_FORMAT_R8G8B8A8_UNORM: 4字节/像素
         * - DXGI_FORMAT_R16G16B16A16_FLOAT: 8字节/像素
         * - DXGI_FORMAT_R32G32B32A32_FLOAT: 16字节/像素
         * - DXGI_FORMAT_R11G11B10_FLOAT: 4字节/像素
         * - 其他格式: 默认4字节/像素
         *
         * 注意事项:
         * - ⚠️ 这是一个估算值，实际内存可能因对齐和系统开销略有不同
         * - ⚠️ 不包括Mipmap链的内存（如果启用）
         * - ⚠️ 不包括MSAA的额外内存（如果启用）
         */
        static size_t CalculateRTMemoryUsage(
            int         width,
            int         height,
            int         colorTexCount,
            DXGI_FORMAT format
        );

        // ========================================================================
        // 配置验证函数组
        // ========================================================================

        /**
         * @brief RT配置验证结果结构体
         *
         * 教学要点:
         * - 使用结构体封装验证结果，提供详细的错误信息
         * - isValid标志快速判断验证结果
         * - errorMessage提供人类可读的错误描述
         * - 支持优化建议（未来扩展）
         */
        struct RTValidationResult
        {
            bool        isValid; ///< 配置是否有效
            std::string errorMessage; ///< 错误信息（如果无效）

            /**
             * @brief 创建有效的验证结果
             * @return RTValidationResult 有效结果
             */
            static RTValidationResult Valid()
            {
                return {true, ""};
            }

            /**
             * @brief 创建无效的验证结果
             * @param message 错误信息
             * @return RTValidationResult 无效结果
             */
            static RTValidationResult Invalid(const std::string& message)
            {
                return {false, message};
            }
        };

        /**
         * @brief 验证RenderTarget配置的有效性
         * @param colorTexCount 颜色纹理数量
         * @param maxColorTextures 最大颜色纹理数量（通常为16）
         * @return RTValidationResult 验证结果
         *
         * 教学要点:
         * - 配置验证模式：在创建资源前验证参数有效性
         * - 早期错误检测：避免在资源创建时才发现错误
         * - 详细错误信息：帮助开发者快速定位问题
         * - 优化建议：提供性能优化的建议（未来扩展）
         *
         * 验证规则:
         * 1. colorTexCount必须 >= 1（至少需要1个RT）
         * 2. colorTexCount必须 <= maxColorTextures（不超过最大限制）
         * 3. maxColorTextures必须 > 0（最大限制必须有效）
         *
         * 示例:
         * @code
         * // 验证有效配置
         * auto result1 = RenderTargetHelper::ValidateRTConfiguration(8, 16);
         * if (result1.isValid) {
         *     std::cout << "Configuration is valid" << std::endl;
         * }
         *
         * // 验证无效配置（超出范围）
         * auto result2 = RenderTargetHelper::ValidateRTConfiguration(20, 16);
         * if (!result2.isValid) {
         *     std::cerr << "Error: " << result2.errorMessage << std::endl;
         *     // 输出: "colorTexCount (20) exceeds maxColorTextures (16)"
         * }
         *
         * // 验证无效配置（小于最小值）
         * auto result3 = RenderTargetHelper::ValidateRTConfiguration(0, 16);
         * if (!result3.isValid) {
         *     std::cerr << "Error: " << result3.errorMessage << std::endl;
         *     // 输出: "colorTexCount (0) is less than minimum required (1)"
         * }
         * @endcode
         *
         * 错误信息格式:
         * - "colorTexCount (X) is less than minimum required (1)"
         * - "colorTexCount (X) exceeds maxColorTextures (Y)"
         * - "maxColorTextures (X) must be greater than 0"
         *
         * 使用场景:
         * - RenderTargetManager构造前验证参数
         * - 配置文件加载后验证配置
         * - 用户输入验证
         * - 单元测试
         *
         * 注意事项:
         * - ⚠️ 这只是参数范围验证，不检查GPU内存是否足够
         * - ⚠️ 不验证具体的RT配置内容（格式、尺寸等）
         * - ⚠️ 调用者需要检查isValid标志并处理错误
         */
        static RTValidationResult ValidateRTConfiguration(
            int colorTexCount,
            int maxColorTextures
        );

        // ========================================================================
        // 配置生成函数组
        // ========================================================================

        /**
         * @brief 生成RenderTarget配置数组
         * @param colorTexCount 需要生成的颜色纹理数量（1-16）
         * @return std::array<RTConfig, 16> RT配置数组（固定16个元素）
         *
         * 教学要点:
         * - 配置生成模式：自动生成标准化的RT配置
         * - 默认配置策略：使用合理的默认值
         * - 占位配置：未使用的槽位填充占位配置
         * - 命名规范：colortex0, colortex1, ..., colortex15
         *
         * 生成策略:
         * 1. 前colorTexCount个配置：生成有效的RT配置
         *    - 名称: "colortex0", "colortex1", ..., "colortex{N-1}"
         *    - 尺寸: 0x0（由RenderTargetManager根据屏幕尺寸计算）
         *    - 格式: DXGI_FORMAT_R8G8B8A8_UNORM
         *    - Flipper: 启用（支持历史帧访问）
         *    - LoadAction: Clear（每帧清空）
         *    - ClearValue: 黑色（Rgba8::BLACK）
         *
         * 2. 剩余配置：生成占位配置
         *    - 名称: "unused_colortex{N}", "unused_colortex{N+1}", ...
         *    - 尺寸: 1x1（最小尺寸）
         *    - 格式: DXGI_FORMAT_R8G8B8A8_UNORM
         *    - Flipper: 禁用
         *    - LoadAction: DontCare（不关心内容）
         *
         * 示例:
         * @code
         * // 生成4个RT的配置
         * auto configs = RenderTargetHelper::GenerateRTConfigs(4);
         * // configs[0] = RTConfig::ColorTarget("colortex0", 0, 0, R8G8B8A8_UNORM, true, Clear, Black)
         * // configs[1] = RTConfig::ColorTarget("colortex1", 0, 0, R8G8B8A8_UNORM, true, Clear, Black)
         * // configs[2] = RTConfig::ColorTarget("colortex2", 0, 0, R8G8B8A8_UNORM, true, Clear, Black)
         * // configs[3] = RTConfig::ColorTarget("colortex3", 0, 0, R8G8B8A8_UNORM, true, Clear, Black)
         * // configs[4-15] = 占位配置（不会被RenderTargetManager使用）
         *
         * // 使用生成的配置创建RenderTargetManager
         * auto rtManager = std::make_unique<RenderTargetManager>(
         *     1920, 1080, configs, 4);
         * @endcode
         *
         * 配置特点:
         * - 尺寸设为0x0：由RenderTargetManager根据baseWidth/baseHeight计算实际尺寸
         * - 统一格式：所有RT使用R8G8B8A8_UNORM（最常用的格式）
         * - 启用Flipper：支持历史帧访问（TAA、Motion Blur等技术需要）
         * - 清空策略：每帧清空为黑色（适合大多数场景）
         *
         * 使用场景:
         * - 快速原型开发：使用默认配置快速搭建渲染管线
         * - 单元测试：生成标准化的测试配置
         * - 配置模板：作为自定义配置的起点
         *
         * 注意事项:
         * - ⚠️ 这是一个便捷方法，生成的是标准化配置
         * - ⚠️ 对于特殊需求（不同格式、尺寸、Mipmap等），需要手动创建配置
         * - ⚠️ 占位配置不会被RenderTargetManager使用，但会占用数组空间
         * - ⚠️ colorTexCount超出范围会被自动修正（1-16）
         */
        static std::array<RTConfig, 16> GenerateRTConfigs(int colorTexCount);

    private:
        /**
         * 教学要点总结：
         * 1. **工具类模式**：纯静态类，所有方法都是static，禁止实例化
         * 2. **功能分组设计**：按职责域组织（内存计算、配置验证、配置生成）
         * 3. **便捷接口封装**：简化常用操作的实现复杂度
         * 4. **配置验证模式**：早期错误检测，提供详细错误信息
         * 5. **内存计算最佳实践**：考虑双缓冲、格式字节数、对齐等因素
         * 6. **默认配置策略**：提供合理的默认值，简化使用
         * 7. **命名规范**：遵循Iris命名约定（colortex0-15）
         * 8. **占位配置**：未使用的槽位填充占位配置，保持数组大小一致
         */
    };
} // namespace enigma::graphic
