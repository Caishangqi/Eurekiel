#pragma once

// 基于 Iris 源码: net.irisshaders.iris.features.FeatureFlags.java
// 文档验证: 2025-09-14 通过 Chrome MCP 直接分析 Iris 源码确认存在
// 
// 教学要点:
// 1. 这个枚举直接对应 Iris 的 FeatureFlags 枚举
// 2. 用于检查硬件和 Iris 对特定渲染特性的支持
// 3. 在创建管线状态时进行特性检查

namespace enigma::graphic
{
    /**
     * @brief 渲染特性标志枚举
     * 
     * 直接对应 Iris FeatureFlags.java 中定义的特性标志
     * 用于在运行时检查硬件和引擎对特定渲染特性的支持
     * 
     * TODO: 基于 Iris 源码实现完整的特性检查逻辑
     * - 实现硬件支持检查 (对应 Iris 的 hardwareRequirement)
     * - 实现引擎支持检查 (对应 Iris 的 irisRequirement) 
     * - 添加 DirectX 12 特有的 BINDLESS_RESOURCES 特性
     */
    enum class FeatureFlags
    {
        // === 基于 Iris FeatureFlags.java 的标准特性 ===
        COMPUTE_SHADERS, // 计算着色器支持
        TESSELLATION_SHADERS, // 细分曲面着色器支持 
        CUSTOM_IMAGES, // 自定义图像支持
        PER_BUFFER_BLENDING, // 每缓冲区混合支持
        SSBO, // 着色器存储缓冲对象支持
        SEPARATE_HARDWARE_SAMPLERS, // 独立硬件采样器
        HIGHER_SHADOWCOLOR, // 高质量阴影颜色
        ENTITY_TRANSLUCENT, // 实体半透明渲染
        REVERSED_CULLING, // 反向剔除
        BLOCK_EMISSION_ATTRIBUTE, // 方块发光属性
        CAN_DISABLE_WEATHER, // 可禁用天气效果

        // === DirectX 12 扩展特性 ===
        BINDLESS_RESOURCES, // Bindless 资源支持 (DirectX 12 特有)

        UNKNOWN // 未知特性标志 (对应 Iris 的 UNKNOWN)
    };

    /**
     * @brief 特性标志检查器
     * 
     * TODO: 实现特性检查逻辑
     * - IsFeatureSupported() - 检查特性是否被支持
     * - GetSupportedFeatures() - 获取所有支持的特性列表
     * - 集成到 IWorldRenderingPipeline::HasFeature() 方法中
     */
    class FeatureFlagsChecker
    {
        // TODO: 基于 Iris 源码实现特性检查逻辑
    };
} // namespace enigma::graphic
