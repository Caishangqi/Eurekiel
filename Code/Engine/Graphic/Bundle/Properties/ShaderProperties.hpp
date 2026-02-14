/**
 * @file ShaderProperties.hpp
 * @brief Shader Pack 全局属性管理器 - 解析 shaders.properties
 * @date 2025-10-12
 *
 * 核心职责:
 * 1. 解析 shaders.properties 文件（基于 Core/Properties.hpp）
 * 2. 管理全局 Shader Pack 配置（阴影、天气、渲染开关等）
 * 3. 提供类型安全的配置访问接口
 * 4. 完全兼容 Iris ShaderProperties.java 架构
 *
 * 参考实现:
 * - Iris: ShaderProperties.java (~985行)
 * - 本项目: Core/Properties.hpp (Java Properties + C 预处理器)
 *
 * 设计特点:
 * - 使用 PropertiesFile 作为底层解析器
 * - 定义 Iris 兼容的数据类型（OptionalBoolean, CloudSetting 等）
 * - 两阶段解析: PropertiesFile 加载 → Directive 分类解析
 * - 约 ~1500-2000 行完整实现
 *
 * 使用示例:
 * ```cpp
 * // 加载 shaders.properties
 * ShaderProperties props(root);
 * if (!props.Parse()) {
 *     ERROR_AND_DIE("Failed to parse shaders.properties");
 * }
 *
 * // 查询配置
 * bool shadowsEnabled = props.GetShadowTerrain().IsTrue();
 * CloudSetting clouds = props.GetCloudSetting();
 * int fallbackTex = props.GetFallbackTex();
 * ```
 */

#pragma once

#include "Engine/Core/Properties.hpp"
#include "Engine/Graphic/Bundle/Texture/CustomTextureData.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>
#include <functional>

namespace enigma::graphic
{
    // ========================================================================
    // 辅助数据类型 - 对应 Iris 的 Java 枚举和数据类
    // ========================================================================

    /**
     * @enum OptionalBoolean
     * @brief 三态布尔值 (Iris OptionalBoolean.java)
     * @details 用于表示可选配置：明确开启、明确关闭、使用默认值
     */
    enum class OptionalBoolean
    {
        DEFAULT, ///< 未指定，使用默认行为
        ENABLED, ///< 明确启用（对应Iris TRUE）
        DISABLED ///< 明确禁用（对应Iris FALSE）
    };

    /**
     * @enum CloudSetting
     * @brief 云渲染设置 (Iris CloudSetting.java)
     */
    enum class CloudSetting
    {
        DEFAULT, ///< 默认（跟随 Minecraft 设置）
        OFF, ///< 禁用云渲染
        FAST, ///< 快速云渲染
        FANCY ///< 精细云渲染
    };

    /**
     * @enum ShadowCullState
     * @brief 阴影剔除设置 (Iris ShadowCullState.java)
     */
    enum class ShadowCullState
    {
        DEFAULT, ///< 默认剔除
        DISTANCE, ///< 距离剔除（false）
        ADVANCED, ///< 高级剔除（true）
        REVERSED ///< 反向剔除（reversed）
    };

    /**
     * @enum ParticleRenderingSettings
     * @brief 粒子渲染设置 (Iris ParticleRenderingSettings.java)
     */
    enum class ParticleRenderingSettings
    {
        UNSET, ///< 未设置
        BEFORE, ///< 延迟渲染前渲染粒子
        MIXED, ///< 混合模式（配合 separateEntityDraws）
        AFTER ///< 延迟渲染后渲染粒子
    };

    /**
     * @struct AlphaTest
     * @brief Alpha 测试覆盖配置 (Iris AlphaTest.java)
     * @details 用于 alphaTest.<pass> = <function> <reference>
     */
    struct AlphaTest
    {
        enum class Function
        {
            NEVER,
            LESS,
            EQUAL,
            LEQUAL,
            GREATER,
            NOTEQUAL,
            GEQUAL,
            ALWAYS
        };

        Function function;
        float    reference;

        AlphaTest(Function func = Function::ALWAYS, float ref = 0.0f)
            : function(func)
              , reference(ref)
        {
        }
    };

    /**
     * @struct ViewportData
     * @brief 视口缩放覆盖 (Iris ViewportData.java)
     * @details 用于 scale.<pass> = <scale> [<offsetX> <offsetY>]
     */
    struct ViewportData
    {
        float scale;
        float offsetX;
        float offsetY;

        ViewportData(float s = 1.0f, float ox = 0.0f, float oy = 0.0f)
            : scale(s)
              , offsetX(ox)
              , offsetY(oy)
        {
        }
    };

    /**
     * @struct TextureScaleOverride
     * @brief 纹理缩放覆盖 (Iris TextureScaleOverride.java)
     * @details 用于 size.buffer.<pass> = <width> <height>
     */
    struct TextureScaleOverride
    {
        std::string width; ///< 可能是像素值或相对比例
        std::string height; ///< 可能是像素值或相对比例

        TextureScaleOverride(const std::string& w = "", const std::string& h = "")
            : width(w)
              , height(h)
        {
        }
    };

    /**
     * @struct BlendModeOverride
     * @brief 混合模式覆盖 (Iris BlendModeOverride.java)
     * @details 用于 blend.<pass> = <srcRGB> <dstRGB> <srcAlpha> <dstAlpha>
     */
    struct BlendModeOverride
    {
        enum class Function
        {
            ZERO,
            ONE,
            SRC_COLOR,
            ONE_MINUS_SRC_COLOR,
            DST_COLOR,
            ONE_MINUS_DST_COLOR,
            SRC_ALPHA,
            ONE_MINUS_SRC_ALPHA,
            DST_ALPHA,
            ONE_MINUS_DST_ALPHA,
            CONSTANT_COLOR,
            ONE_MINUS_CONSTANT_COLOR,
            CONSTANT_ALPHA,
            ONE_MINUS_CONSTANT_ALPHA,
            SRC_ALPHA_SATURATE,
            OFF ///< 特殊值：禁用混合
        };

        Function srcRGB;
        Function dstRGB;
        Function srcAlpha;
        Function dstAlpha;

        BlendModeOverride(Function sr = Function::ONE,
                          Function dr = Function::ZERO,
                          Function sa = Function::ONE,
                          Function da = Function::ZERO)
            : srcRGB(sr)
              , dstRGB(dr)
              , srcAlpha(sa)
              , dstAlpha(da)
        {
        }

        static BlendModeOverride Off();
    };

    /**
     * @struct IndirectPointer
     * @brief 间接绘制指针 (Iris IndirectPointer.java)
     * @details 用于 indirect.<pass> = <bufferIndex> <offset>
     */
    struct IndirectPointer
    {
        int  bufferIndex;
        long offset;

        IndirectPointer(int idx = 0, long off = 0)
            : bufferIndex(idx)
              , offset(off)
        {
        }
    };

    /**
     * @struct BufferBlendInformation
     * @brief 单个缓冲区混合信息 (Iris BufferBlendInformation.java)
     * @details 用于 blend.<pass>.<buffer> = <mode>
     */
    struct BufferBlendInformation
    {
        int                              bufferIndex; ///< colortex 索引
        std::optional<BlendModeOverride> blendMode; ///< 混合模式（nullopt = off）

        BufferBlendInformation(int idx, const std::optional<BlendModeOverride>& mode)
            : bufferIndex(idx)
              , blendMode(mode)
        {
        }
    };

    /**
     * @struct ShaderStorageInfo
     * @brief Shader Storage Buffer Object 配置 (Iris ShaderStorageInfo.java)
     * @details 用于 bufferObject.<index> = <size> [relative] [scaleX scaleY] [name]
     */
    struct ShaderStorageInfo
    {
        long        size; ///< 缓冲区大小（字节）
        bool        isRelative; ///< 是否相对于屏幕尺寸
        float       scaleX; ///< X 缩放因子
        float       scaleY; ///< Y 缩放因子
        std::string name; ///< 可选名称

        ShaderStorageInfo(long               sz  = 0,
                          bool               rel = false,
                          float              sx  = 0.0f,
                          float              sy  = 0.0f,
                          const std::string& n   = "")
            : size(sz)
              , isRelative(rel)
              , scaleX(sx)
              , scaleY(sy)
              , name(n)
        {
        }
    };

    // ========================================================================
    // ShaderProperties 主类
    // ========================================================================

    /**
     * @class ShaderProperties
     * @brief Shader Pack 全局属性管理器
     *
     * 功能特性:
     * 1. **基础配置解析**:
     *    - Boolean 配置（~40 个）: oldHandLight, shadowTerrain, oldLighting, weather 等
     *    - Int 配置: fallbackTex
     *    - String 配置: texture.noise
     *    - Enum 配置: clouds, dhClouds, shadow.culling, particles.ordering
     *
     * 2. **Per-Pass 覆盖**:
     *    - scale.<pass> = <scale> [<offsetX> <offsetY>]
     *    - alphaTest.<pass> = <function> <reference>
     *    - blend.<pass> = <srcRGB> <dstRGB> <srcAlpha> <dstAlpha>
     *    - size.buffer.<pass> = <width> <height>
     *    - indirect.<pass> = <bufferIndex> <offset>
     *
     * 3. **纹理和图像配置**:
     *    - texture.<stage>.<sampler> = <path>
     *    - customTexture.<name> = <path> [<type> <internalFormat> ...]
     *    - image.<name> = <samplerName> <format> <internalFormat> <pixelType> <clear> <relative> [<size>]
     *
     * 4. **Buffer Object 配置**:
     *    - bufferObject.<index> = <size> [relative] [scaleX scaleY] [name]
     *
     * 5. **UI/Screen 配置**:
     *    - sliders = <option1> <option2> ...
     *    - screen = <option1> <option2> ...
     *    - screen.columns = <count>
     *    - profile.<name> = <option1> <option2> ...
     *
     * 6. **Feature Flags**:
     *    - iris.features.required = <flag1> <flag2> ...
     *    - iris.features.optional = <flag1> <flag2> ...
     *
     * 7. **Custom Uniforms/Variables**:
     *    - uniform.<type>.<name> = <expression>
     *    - variable.<type>.<name> = <expression>
     *
     * 8. **Flip Directives**:
     *    - flip.<pass>.<buffer> = <true|false>
     *
     * 9. **Conditional Program Enabling**:
     *    - program.<name>.enabled = <condition>
     *
     * 教学要点:
     * - 两阶段解析: PropertiesFile 加载 → Directive 分类处理
     * - Helper 函数模式: HandleBooleanDirective(), HandlePassDirective() 等
     * - 配置覆盖机制: 全局默认值 + Per-Pass 覆盖
     */
    class ShaderProperties
    {
    public:
        ShaderProperties();
        ~ShaderProperties() = default;

        // 禁用拷贝
        ShaderProperties(const ShaderProperties&)            = delete;
        ShaderProperties& operator=(const ShaderProperties&) = delete;

        // 支持移动
        ShaderProperties(ShaderProperties&&) noexcept            = default;
        ShaderProperties& operator=(ShaderProperties&&) noexcept = default;

        // ========================================================================
        // 加载和解析
        // ========================================================================

        /**
         * @brief 从 Shader Pack 根目录加载 shaders.properties
         * @param rootPath Shader Pack 根目录（例如: ".enigma/shaderpacks/ComplementaryReimagined"）
         * @return true 成功, false 失败
         * @details 实际加载路径: rootPath / "shaders" / "shaders.properties"
         */
        bool Parse(const std::filesystem::path& rootPath);

        /**
         * @brief 从字符串内容加载（用于测试）
         * @param content shaders.properties 文件内容
         * @return true 成功, false 失败
         */
        bool ParseFromString(const std::string& content);

        /**
         * @brief 检查是否已成功解析
         * @return true 已解析, false 未解析
         */
        bool IsValid() const;

        // ========================================================================
        // Boolean 配置 Getters (~40 个)
        // ========================================================================

        OptionalBoolean GetOldHandLight() const { return m_oldHandLight; }
        OptionalBoolean GetDynamicHandLight() const { return m_dynamicHandLight; }
        OptionalBoolean GetOldLighting() const { return m_oldLighting; }
        OptionalBoolean GetShadowTerrain() const { return m_shadowTerrain; }
        OptionalBoolean GetShadowTranslucent() const { return m_shadowTranslucent; }
        OptionalBoolean GetShadowEntities() const { return m_shadowEntities; }
        OptionalBoolean GetShadowPlayer() const { return m_shadowPlayer; }
        OptionalBoolean GetShadowBlockEntities() const { return m_shadowBlockEntities; }
        OptionalBoolean GetShadowLightBlockEntities() const { return m_shadowLightBlockEntities; }
        OptionalBoolean GetUnderwaterOverlay() const { return m_underwaterOverlay; }
        OptionalBoolean GetSun() const { return m_sun; }
        OptionalBoolean GetMoon() const { return m_moon; }
        OptionalBoolean GetStars() const { return m_stars; }
        OptionalBoolean GetSky() const { return m_sky; }
        OptionalBoolean GetVignette() const { return m_vignette; }
        OptionalBoolean GetBackFaceSolid() const { return m_backFaceSolid; }
        OptionalBoolean GetBackFaceCutout() const { return m_backFaceCutout; }
        OptionalBoolean GetBackFaceCutoutMipped() const { return m_backFaceCutoutMipped; }
        OptionalBoolean GetBackFaceTranslucent() const { return m_backFaceTranslucent; }
        OptionalBoolean GetRainDepth() const { return m_rainDepth; }
        OptionalBoolean GetConcurrentCompute() const { return m_concurrentCompute; }
        OptionalBoolean GetBeaconBeamDepth() const { return m_beaconBeamDepth; }
        OptionalBoolean GetSeparateAo() const { return m_separateAo; }
        OptionalBoolean GetVoxelizeLightBlocks() const { return m_voxelizeLightBlocks; }
        OptionalBoolean GetSeparateEntityDraws() const { return m_separateEntityDraws; }
        OptionalBoolean GetSkipAllRendering() const { return m_skipAllRendering; }
        OptionalBoolean GetFrustumCulling() const { return m_frustumCulling; }
        OptionalBoolean GetOcclusionCulling() const { return m_occlusionCulling; }
        OptionalBoolean GetShadowEnabled() const { return m_shadowEnabled; }
        OptionalBoolean GetDhShadowEnabled() const { return m_dhShadowEnabled; }
        OptionalBoolean GetPrepareBeforeShadow() const { return m_prepareBeforeShadow; }
        OptionalBoolean GetSupportsColorCorrection() const { return m_supportsColorCorrection; }

        OptionalBoolean GetWeather() const { return m_weather; }
        OptionalBoolean GetWeatherParticles() const { return m_weatherParticles; }

        // ========================================================================
        // Enum 配置 Getters
        // ========================================================================

        CloudSetting              GetCloudSetting() const { return m_cloudSetting; }
        CloudSetting              GetDhCloudSetting() const { return m_dhCloudSetting; }
        ShadowCullState           GetShadowCulling() const { return m_shadowCulling; }
        ParticleRenderingSettings GetParticleRenderingSettings() const;

        // ========================================================================
        // 标量配置 Getters
        // ========================================================================

        int GetFallbackTex() const { return m_fallbackTex; }

        std::optional<std::string> GetNoiseTexturePath() const
        {
            return m_noiseTexturePath.empty()
                       ? std::nullopt
                       : std::optional<std::string>(m_noiseTexturePath);
        }

        // ========================================================================
        // Per-Pass 覆盖 Getters
        // ========================================================================

        const std::unordered_map<std::string, AlphaTest>&            GetAlphaTestOverrides() const;
        const std::unordered_map<std::string, ViewportData>&         GetViewportScaleOverrides() const;
        const std::unordered_map<std::string, TextureScaleOverride>& GetTextureScaleOverrides() const;
        const std::unordered_map<std::string, BlendModeOverride>&    GetBlendModeOverrides() const;
        const std::unordered_map<std::string, IndirectPointer>&      GetIndirectPointers() const;
        const std::unordered_map<std::string, std::vector<BufferBlendInformation>>&
        GetBufferBlendOverrides() const;

        // ========================================================================
        // Buffer Object Getters
        // ========================================================================

        const std::unordered_map<int, ShaderStorageInfo>& GetBufferObjects() const;

        // ========================================================================
        // UI/Screen 配置 Getters
        // ========================================================================

        const std::vector<std::string>&                                  GetSliderOptions() const;
        const std::unordered_map<std::string, std::vector<std::string>>& GetProfiles() const;
        std::optional<std::vector<std::string>>                          GetMainScreenOptions() const;
        const std::unordered_map<std::string, std::vector<std::string>>& GetSubScreenOptions() const;
        std::optional<int>                                               GetMainScreenColumnCount() const;
        const std::unordered_map<std::string, int>&                      GetSubScreenColumnCount() const;

        // ========================================================================
        // Feature Flags Getters
        // ========================================================================

        const std::vector<std::string>& GetRequiredFeatureFlags() const;
        const std::vector<std::string>& GetOptionalFeatureFlags() const;

        // ========================================================================
        // Flip Directives Getters
        // ========================================================================

        const std::unordered_map<std::string, std::unordered_map<std::string, bool>>& GetExplicitFlips() const;
        // ========================================================================
        // Conditional Program Enabling Getters
        // ========================================================================

        const std::unordered_map<std::string, std::string>& GetConditionallyEnabledPrograms() const;

        // ========================================================================
        // Custom Texture Data Getter
        // ========================================================================

        const CustomTextureData& GetCustomTextureData() const { return m_customTextureData; }

    private:
        // ========================================================================
        // 内部解析方法
        // ========================================================================

        /**
         * @brief 解析单个键值对
         * @param key 键名
         * @param value 值
         */
        void ParseDirective(const std::string& key, const std::string& value);

        /**
         * @brief 处理 Boolean Directive
         * @param key 键名
         * @param value 值
         * @param expectedKey 期望的键名
         * @param target 目标成员变量引用
         */
        void HandleBooleanDirective(const std::string& key,
                                    const std::string& value,
                                    const std::string& expectedKey,
                                    OptionalBoolean&   target);

        /**
         * @brief 处理 Int Directive
         * @param key 键名
         * @param value 值
         * @param expectedKey 期望的键名
         * @param target 目标成员变量引用
         * @return true 匹配并处理, false 不匹配
         */
        bool HandleIntDirective(const std::string& key,
                                const std::string& value,
                                const std::string& expectedKey,
                                int&               target);

        /**
         * @brief 处理 Pass Directive (例如: scale.<pass>, alphaTest.<pass>)
         * @param prefix 前缀（例如: "scale."）
         * @param key 键名
         * @param value 值
         * @param handler 处理函数（参数: pass 名称）
         */
        void HandlePassDirective(const std::string&                      prefix,
                                 const std::string&                      key,
                                 const std::string&                      value,
                                 std::function<void(const std::string&)> handler);

        /**
         * @brief 从字符串解析 Boolean 值
         * @param value 字符串值 ("true", "false", "1", "0")
         * @return OptionalBoolean 结果
         */
        static OptionalBoolean ParseBooleanValue(const std::string& value);

        /**
         * @brief 从字符串解析 AlphaTest.Function
         * @param value 字符串值 (例如: "GREATER")
         * @return Function 枚举值
         */
        static AlphaTest::Function ParseAlphaTestFunction(const std::string& value);

        /**
         * @brief 从字符串解析 BlendModeOverride.Function
         * @param value 字符串值 (例如: "SRC_ALPHA")
         * @return Function 枚举值
         */
        static BlendModeOverride::Function ParseBlendModeFunction(const std::string& value);

        /**
         * @brief 分割空格分隔的列表
         * @param value 字符串值 (例如: "option1 option2 option3")
         * @return 字符串列表
         */
        static std::vector<std::string> SplitWhitespace(const std::string& value);

        // Custom texture directive handlers
        void               HandleTextureStageDirective(const std::string& key, const std::string& value);
        void               HandleCustomTextureDirective(const std::string& key, const std::string& value);
        TextureDeclaration ParseTexturePath(const std::string& rawValue);
        static bool        IsValidStage(const std::string& stage);
        static int         ParseTextureSlot(const std::string& slotStr);

    private:
        // ========================================================================
        // 成员变量 - Boolean 配置 (~40 个)
        // ========================================================================

        OptionalBoolean m_oldHandLight             = OptionalBoolean::DEFAULT;
        OptionalBoolean m_dynamicHandLight         = OptionalBoolean::DEFAULT;
        OptionalBoolean m_oldLighting              = OptionalBoolean::DEFAULT;
        OptionalBoolean m_shadowTerrain            = OptionalBoolean::DEFAULT;
        OptionalBoolean m_shadowTranslucent        = OptionalBoolean::DEFAULT;
        OptionalBoolean m_shadowEntities           = OptionalBoolean::DEFAULT;
        OptionalBoolean m_shadowPlayer             = OptionalBoolean::DEFAULT;
        OptionalBoolean m_shadowBlockEntities      = OptionalBoolean::DEFAULT;
        OptionalBoolean m_shadowLightBlockEntities = OptionalBoolean::DEFAULT;
        OptionalBoolean m_underwaterOverlay        = OptionalBoolean::DEFAULT;
        OptionalBoolean m_sun                      = OptionalBoolean::DEFAULT;
        OptionalBoolean m_moon                     = OptionalBoolean::DEFAULT;
        OptionalBoolean m_stars                    = OptionalBoolean::DEFAULT;
        OptionalBoolean m_sky                      = OptionalBoolean::DEFAULT;
        OptionalBoolean m_vignette                 = OptionalBoolean::DEFAULT;
        OptionalBoolean m_backFaceSolid            = OptionalBoolean::DEFAULT;
        OptionalBoolean m_backFaceCutout           = OptionalBoolean::DEFAULT;
        OptionalBoolean m_backFaceCutoutMipped     = OptionalBoolean::DEFAULT;
        OptionalBoolean m_backFaceTranslucent      = OptionalBoolean::DEFAULT;
        OptionalBoolean m_rainDepth                = OptionalBoolean::DEFAULT;
        OptionalBoolean m_concurrentCompute        = OptionalBoolean::DEFAULT;
        OptionalBoolean m_beaconBeamDepth          = OptionalBoolean::DEFAULT;
        OptionalBoolean m_separateAo               = OptionalBoolean::DEFAULT;
        OptionalBoolean m_voxelizeLightBlocks      = OptionalBoolean::DEFAULT;
        OptionalBoolean m_separateEntityDraws      = OptionalBoolean::DEFAULT;
        OptionalBoolean m_skipAllRendering         = OptionalBoolean::DEFAULT;
        OptionalBoolean m_frustumCulling           = OptionalBoolean::DEFAULT;
        OptionalBoolean m_occlusionCulling         = OptionalBoolean::DEFAULT;
        OptionalBoolean m_shadowEnabled            = OptionalBoolean::DEFAULT;
        OptionalBoolean m_dhShadowEnabled          = OptionalBoolean::DEFAULT;
        OptionalBoolean m_prepareBeforeShadow      = OptionalBoolean::DEFAULT;
        OptionalBoolean m_supportsColorCorrection  = OptionalBoolean::DEFAULT;
        OptionalBoolean m_weather                  = OptionalBoolean::DEFAULT;
        OptionalBoolean m_weatherParticles         = OptionalBoolean::DEFAULT;
        OptionalBoolean m_shadowFrustumCulling     = OptionalBoolean::DEFAULT;
        OptionalBoolean m_customImages             = OptionalBoolean::DEFAULT;
        OptionalBoolean m_customTextures           = OptionalBoolean::DEFAULT;
        OptionalBoolean m_customEntityModels       = OptionalBoolean::DEFAULT;
        OptionalBoolean m_customBlockEntities      = OptionalBoolean::DEFAULT;
        OptionalBoolean m_customUniforms           = OptionalBoolean::DEFAULT;
        OptionalBoolean m_entityAttrib             = OptionalBoolean::DEFAULT;
        OptionalBoolean m_midTexCoordAttrib        = OptionalBoolean::DEFAULT;
        OptionalBoolean m_tangentAttrib            = OptionalBoolean::DEFAULT;
        OptionalBoolean m_beacon                   = OptionalBoolean::DEFAULT;
        OptionalBoolean m_separateHardwareSamplers = OptionalBoolean::DEFAULT;

        // ========================================================================
        // 成员变量 - Enum 配置
        // ========================================================================

        CloudSetting              m_cloudSetting              = CloudSetting::DEFAULT;
        CloudSetting              m_dhCloudSetting            = CloudSetting::DEFAULT;
        ShadowCullState           m_shadowCulling             = ShadowCullState::DEFAULT;
        ParticleRenderingSettings m_particleRenderingSettings = ParticleRenderingSettings::UNSET;

        // ========================================================================
        // 成员变量 - 标量配置
        // ========================================================================

        int         m_fallbackTex      = 0;
        std::string m_noiseTexturePath = "";

        // ========================================================================
        // 成员变量 - Per-Pass 覆盖 Maps
        // ========================================================================

        std::unordered_map<std::string, AlphaTest>                           m_alphaTestOverrides;
        std::unordered_map<std::string, ViewportData>                        m_viewportScaleOverrides;
        std::unordered_map<std::string, TextureScaleOverride>                m_textureScaleOverrides;
        std::unordered_map<std::string, BlendModeOverride>                   m_blendModeOverrides;
        std::unordered_map<std::string, IndirectPointer>                     m_indirectPointers;
        std::unordered_map<std::string, std::vector<BufferBlendInformation>> m_bufferBlendOverrides;

        // ========================================================================
        // 成员变量 - Buffer Objects
        // ========================================================================

        std::unordered_map<int, ShaderStorageInfo> m_bufferObjects;

        // ========================================================================
        // 成员变量 - UI/Screen 配置
        // ========================================================================

        std::vector<std::string>                                  m_sliderOptions;
        std::unordered_map<std::string, std::vector<std::string>> m_profiles;
        std::vector<std::string>                                  m_mainScreenOptions;
        std::unordered_map<std::string, std::vector<std::string>> m_subScreenOptions;
        int                                                       m_mainScreenColumnCount = -1;
        std::unordered_map<std::string, int>                      m_subScreenColumnCount;

        // ========================================================================
        // 成员变量 - Feature Flags
        // ========================================================================

        std::vector<std::string> m_requiredFeatureFlags;
        std::vector<std::string> m_optionalFeatureFlags;

        // ========================================================================
        // 成员变量 - Flip Directives
        // ========================================================================

        std::unordered_map<std::string, std::unordered_map<std::string, bool>> m_explicitFlips;

        // ========================================================================
        // 成员变量 - Conditional Program Enabling
        // ========================================================================

        std::unordered_map<std::string, std::string> m_conditionallyEnabledPrograms;

        // ========================================================================
        // Custom Texture Data
        // ========================================================================

        CustomTextureData m_customTextureData;

        // ========================================================================
        // Internal State
        // ========================================================================

        bool m_isValid = false; ///< 是否成功解析
    };
} // namespace enigma::graphic
