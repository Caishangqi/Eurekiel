/**
 * @file ShaderProperties.cpp
 * @brief Shader Pack 全局属性管理器实现 - 解析 shaders.properties
 * @date 2025-10-12
 *
 * 实现要点:
 * 1. 使用 PropertiesFile 作为底层解析器（继承自 Core/Properties.hpp）
 * 2. ParseDirective() 为核心分发器，处理~40个boolean配置、enum配置、per-pass覆盖等
 * 3. Helper方法模式: HandleBooleanDirective(), HandlePassDirective() 等
 * 4. 完全兼容 Iris ShaderProperties.java (~985行) 架构
 *
 * 参考实现:
 * - Iris: ShaderProperties.java (真实源码对齐)
 * - 本项目: Core/Properties.hpp (Java Properties + C 预处理器)
 *
 * 教学价值:
 * - 两阶段解析: PropertiesFile 加载 → Directive 分类处理
 * - Lambda表达式应用: 避免std::transform警告
 * - 类型安全设计: OptionalBoolean, CloudSetting 等枚举类型
 * - 错误处理策略: 解析失败使用默认值（不抛异常，对齐Iris行为）
 */

#include "ShaderProperties.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Graphic/Bundle/BundleException.hpp"

using namespace enigma::graphic;

// ============================================================================
// Constructor
// ============================================================================

ShaderProperties::ShaderProperties()
{
    // 所有成员变量已在头文件中初始化为默认值
}

// ============================================================================
// 加载和解析
// ============================================================================

/**
 * @brief 从 Shader Pack 根目录加载 shaders.properties
 * @details 实际路径: rootPath / "shaders" / "shaders.properties"
 *
 * 流程:
 * 1. 构造完整路径
 * 2. 使用 PropertiesFile::Load() 加载文件
 * 3. 遍历所有键值对，调用 ParseDirective()
 * 4. 设置 m_isValid = true
 */
bool ShaderProperties::Parse(const std::filesystem::path& rootPath)
{
    // 构造 shaders.properties 文件路径
    auto propertiesPath = rootPath / "shaders" / "shaders.properties";

    // 使用 PropertiesFile 加载
    enigma::core::PropertiesFile props;
    if (!props.Load(propertiesPath))
    {
        return false;
    }

    // 遍历所有键值对，解析指令
    for (const auto& [key, value] : props.GetAll())
    {
        ParseDirective(key, value);
    }

    m_isValid = true;
    return true;
}

/**
 * @brief 从字符串内容加载（用于测试）
 * @details 直接解析字符串内容，无需文件IO
 *
 * 使用场景:
 * - 单元测试
 * - 内存中的配置生成
 */
bool ShaderProperties::ParseFromString(const std::string& content)
{
    // 使用 PropertiesFile 从字符串加载
    enigma::core::PropertiesFile props;
    if (!props.LoadFromString(content))
    {
        return false;
    }

    // 遍历所有键值对，解析指令
    for (const auto& [key, value] : props.GetAll())
    {
        ParseDirective(key, value);
    }

    m_isValid = true;
    return true;
}

bool ShaderProperties::IsValid() const
{
    return m_isValid;
}

// ============================================================================
// 主解析逻辑
// ============================================================================

/**
 * @brief 核心分发器：解析单个键值对
 * @param key 键名（例如: "oldHandLight", "scale.composite", "clouds"）
 * @param value 值（例如: "true", "2.0 0.5 0.5", "fancy"）
 *
 * 设计思路:
 * 1. 使用 if/else 链（而非 unordered_map）以便清晰教学
 * 2. 优先处理 Boolean 指令（~40个）
 * 3. 然后处理 Enum/Scalar 指令
 * 4. 最后处理 Per-Pass 覆盖（前缀匹配）
 *
 * 错误处理:
 * - 解析失败时使用默认值（不抛异常）
 * - 对齐 Iris 行为：静默忽略未知指令
 */
void ShaderProperties::ParseDirective(const std::string& key, const std::string& value)
{
    // ========================================================================
    // 1. Boolean 指令 (~40 个)
    // ========================================================================
    HandleBooleanDirective(key, value, "oldHandLight", m_oldHandLight);
    HandleBooleanDirective(key, value, "dynamicHandLight", m_dynamicHandLight);
    HandleBooleanDirective(key, value, "oldLighting", m_oldLighting);
    HandleBooleanDirective(key, value, "shadowTerrain", m_shadowTerrain);
    HandleBooleanDirective(key, value, "shadowTranslucent", m_shadowTranslucent);
    HandleBooleanDirective(key, value, "shadowEntities", m_shadowEntities);
    HandleBooleanDirective(key, value, "shadowPlayer", m_shadowPlayer);
    HandleBooleanDirective(key, value, "shadowBlockEntities", m_shadowBlockEntities);
    HandleBooleanDirective(key, value, "shadowLightBlockEntities", m_shadowLightBlockEntities); // 修复: 新增
    HandleBooleanDirective(key, value, "underwaterOverlay", m_underwaterOverlay);
    HandleBooleanDirective(key, value, "sun", m_sun);
    HandleBooleanDirective(key, value, "moon", m_moon);
    HandleBooleanDirective(key, value, "stars", m_stars); // 修复: 新增
    HandleBooleanDirective(key, value, "sky", m_sky); // 修复: 新增
    HandleBooleanDirective(key, value, "vignette", m_vignette);
    HandleBooleanDirective(key, value, "backFaceSolid", m_backFaceSolid);
    HandleBooleanDirective(key, value, "backFaceCutout", m_backFaceCutout);
    HandleBooleanDirective(key, value, "backFaceCutoutMipped", m_backFaceCutoutMipped);
    HandleBooleanDirective(key, value, "backFaceTranslucent", m_backFaceTranslucent);
    HandleBooleanDirective(key, value, "separateAo", m_separateAo);
    HandleBooleanDirective(key, value, "voxelizeLightBlocks", m_voxelizeLightBlocks); // 修复: 新增
    HandleBooleanDirective(key, value, "separateEntityDraws", m_separateEntityDraws);
    HandleBooleanDirective(key, value, "rainDepth", m_rainDepth);
    HandleBooleanDirective(key, value, "beaconBeamDepth", m_beaconBeamDepth);
    HandleBooleanDirective(key, value, "prepareBeforeShadow", m_prepareBeforeShadow);
    HandleBooleanDirective(key, value, "skipAllRendering", m_skipAllRendering);
    HandleBooleanDirective(key, value, "frustumCulling", m_frustumCulling);
    HandleBooleanDirective(key, value, "shadowFrustumCulling", m_shadowFrustumCulling);
    HandleBooleanDirective(key, value, "occlusionCulling", m_occlusionCulling); // 修复: 新增
    HandleBooleanDirective(key, value, "shadowEnabled", m_shadowEnabled); // 修复: 新增
    HandleBooleanDirective(key, value, "dhShadowEnabled", m_dhShadowEnabled); // 修复: 新增
    HandleBooleanDirective(key, value, "supportsColorCorrection", m_supportsColorCorrection);
    HandleBooleanDirective(key, value, "concurrentCompute", m_concurrentCompute);
    HandleBooleanDirective(key, value, "customImages", m_customImages);
    HandleBooleanDirective(key, value, "customTextures", m_customTextures);
    HandleBooleanDirective(key, value, "customEntityModels", m_customEntityModels);
    HandleBooleanDirective(key, value, "customBlockEntities", m_customBlockEntities);
    HandleBooleanDirective(key, value, "customUniforms", m_customUniforms);
    HandleBooleanDirective(key, value, "entityAttrib", m_entityAttrib);
    HandleBooleanDirective(key, value, "midTexCoordAttrib", m_midTexCoordAttrib);
    HandleBooleanDirective(key, value, "tangentAttrib", m_tangentAttrib);
    HandleBooleanDirective(key, value, "beacon", m_beacon);
    HandleBooleanDirective(key, value, "separateHardwareSamplers", m_separateHardwareSamplers);
    HandleBooleanDirective(key, value, "weather", m_weather);
    HandleBooleanDirective(key, value, "weatherParticles", m_weatherParticles);

    // ========================================================================
    // 2. Enum 指令
    // ========================================================================

    // clouds 指令
    // 格式: clouds = off | fast | fancy | default
    if (key == "clouds")
    {
        if (value == "off")
        {
            m_cloudSetting = CloudSetting::OFF;
        }
        else if (value == "fast")
        {
            m_cloudSetting = CloudSetting::FAST;
        }
        else if (value == "fancy")
        {
            m_cloudSetting = CloudSetting::FANCY;
        }
        else if (value == "default")
        {
            m_cloudSetting = CloudSetting::DEFAULT;
        }
    }

    // dhClouds 指令（Distant Horizons 云渲染设置）
    // 格式: dhClouds = off | fast | fancy | default
    if (key == "dhClouds")
    {
        if (value == "off")
        {
            m_dhCloudSetting = CloudSetting::OFF;
        }
        else if (value == "fast")
        {
            m_dhCloudSetting = CloudSetting::FAST;
        }
        else if (value == "fancy")
        {
            m_dhCloudSetting = CloudSetting::FANCY;
        }
        else if (value == "default")
        {
            m_dhCloudSetting = CloudSetting::DEFAULT;
        }
    }

    // shadowCulling 指令
    // 格式: shadowCulling = off | distance | advanced | reversed | default
    if (key == "shadowCulling")
    {
        if (value == "off" || value == "distance")
        {
            m_shadowCulling = ShadowCullState::DISTANCE;
        }
        else if (value == "advanced")
        {
            m_shadowCulling = ShadowCullState::ADVANCED;
        }
        else if (value == "reversed")
        {
            m_shadowCulling = ShadowCullState::REVERSED;
        }
        else if (value == "default")
        {
            m_shadowCulling = ShadowCullState::DEFAULT;
        }
    }

    // particleRendering 指令
    // 格式: particleRendering = before | mixed | after
    if (key == "particleRendering")
    {
        if (value == "before")
        {
            m_particleRenderingSettings = ParticleRenderingSettings::BEFORE;
        }
        else if (value == "mixed")
        {
            m_particleRenderingSettings = ParticleRenderingSettings::MIXED;
        }
        else if (value == "after")
        {
            m_particleRenderingSettings = ParticleRenderingSettings::AFTER;
        }
    }

    // ========================================================================
    // 3. 标量指令
    // ========================================================================

    // fallbackTex 指令
    // 格式: fallbackTex = <index>
    if (HandleIntDirective(key, value, "fallbackTex", m_fallbackTex))
    {
        return;
    }

    // texture.noise 指令（特殊处理：原Iris中为 noiseTextureResolution）
    // 格式: texture.noise = <path>
    if (key == "noiseTextureResolution" || key == "texture.noise")
    {
        m_noiseTexturePath = value;
    }

    // ========================================================================
    // 4. Per-Pass 覆盖指令
    // ========================================================================

    // scale.<pass> 指令
    // 格式: scale.<pass> = <scale> [offsetX] [offsetY]
    // 示例: scale.composite = 0.5 0.0 0.0
    HandlePassDirective("scale.", key, value, [this, &value](const std::string& pass)
    {
        auto parts = SplitWhitespace(value);
        if (parts.empty())
        {
            return;
        }

        try
        {
            float scale   = std::stof(parts[0]);
            float offsetX = 0.0f;
            float offsetY = 0.0f;

            if (parts.size() > 2)
            {
                offsetX = std::stof(parts[1]);
                offsetY = std::stof(parts[2]);
            }

            m_viewportScaleOverrides[pass] = ViewportData{scale, offsetX, offsetY};
        }
        catch (...)
        {
            // 解析失败，忽略
        }
    });

    // size.buffer.<index> 指令
    // 格式: size.buffer.<index> = <width> <height>
    // 示例: size.buffer.colortex0 = 1920 1080
    HandlePassDirective("size.buffer.", key, value, [this, &value](const std::string& index)
    {
        auto parts = SplitWhitespace(value);
        if (parts.size() != 2)
        {
            return;
        }

        m_textureScaleOverrides[index] = TextureScaleOverride{parts[0], parts[1]};
    });

    // alphaTest.<pass> 指令
    // 格式1: alphaTest.<pass> = <function> <reference>
    // 格式2: alphaTest.<pass> = off | false
    // 示例: alphaTest.gbuffers_terrain = GREATER 0.1
    HandlePassDirective("alphaTest.", key, value, [this, &value](const std::string& pass)
    {
        if (value == "off" || value == "false")
        {
            m_alphaTestOverrides[pass] = AlphaTest{AlphaTest::Function::ALWAYS, 0.0f};
            return;
        }

        auto parts = SplitWhitespace(value);
        if (parts.size() < 2)
        {
            return;
        }

        try
        {
            AlphaTest::Function function  = ParseAlphaTestFunction(parts[0]);
            float               reference = std::stof(parts[1]);
            m_alphaTestOverrides[pass]    = AlphaTest{function, reference};
        }
        catch (...)
        {
            // 解析失败，忽略
        }
    });

    // blend.<pass> 指令（支持普通blend和buffer blend）
    // 格式1: blend.<pass> = <srcRGB> <dstRGB> <srcAlpha> <dstAlpha> 或 off
    // 格式2: blend.<pass>.<buffer> = <srcRGB> <dstRGB> <srcAlpha> <dstAlpha> 或 off
    // 示例1: blend.composite = SRC_ALPHA ONE_MINUS_SRC_ALPHA SRC_ALPHA ONE
    // 示例2: blend.composite.colortex0 = off
    HandlePassDirective("blend.", key, value, [this, &value](const std::string& passAndBuffer)
    {
        // 检查是否为buffer blend（包含点号）
        size_t dotPos = passAndBuffer.find('.');
        if (dotPos != std::string::npos)
        {
            // Buffer blend: blend.<pass>.<buffer>
            std::string pass   = passAndBuffer.substr(0, dotPos);
            std::string buffer = passAndBuffer.substr(dotPos + 1);

            // 解析buffer索引
            int bufferIndex = -1;
            if (buffer.rfind("colortex", 0) == 0)
            {
                try
                {
                    bufferIndex = std::stoi(buffer.substr(8));
                }
                catch (...)
                {
                    return;
                }
            }

            if (bufferIndex == -1)
            {
                return;
            }

            if (value == "off")
            {
                m_bufferBlendOverrides[pass].push_back(BufferBlendInformation{bufferIndex, std::nullopt});
                return;
            }

            auto parts = SplitWhitespace(value);
            if (parts.size() < 4)
            {
                return;
            }

            BlendModeOverride blendMode{
                ParseBlendModeFunction(parts[0]),
                ParseBlendModeFunction(parts[1]),
                ParseBlendModeFunction(parts[2]),
                ParseBlendModeFunction(parts[3])
            };

            m_bufferBlendOverrides[pass].push_back(BufferBlendInformation{bufferIndex, blendMode});
            return;
        }

        // 普通blend: blend.<pass>
        if (value == "off")
        {
            m_blendModeOverrides[passAndBuffer] = BlendModeOverride::Off();
            return;
        }

        auto parts = SplitWhitespace(value);
        if (parts.size() < 4)
        {
            return;
        }

        m_blendModeOverrides[passAndBuffer] = BlendModeOverride{
            ParseBlendModeFunction(parts[0]),
            ParseBlendModeFunction(parts[1]),
            ParseBlendModeFunction(parts[2]),
            ParseBlendModeFunction(parts[3])
        };
    });

    // indirect.<pass> 指令
    // 格式: indirect.<pass> = <bufferIndex> <offset>
    // 示例: indirect.composite = 0 1024
    HandlePassDirective("indirect.", key, value, [this, &value](const std::string& pass)
    {
        auto parts = SplitWhitespace(value);
        if (parts.size() != 2)
        {
            return;
        }

        try
        {
            int  bufferIndex         = std::stoi(parts[0]);
            long offset              = std::stol(parts[1]);
            m_indirectPointers[pass] = IndirectPointer{bufferIndex, offset};
        }
        catch (...)
        {
            // 解析失败，忽略
        }
    });

    // bufferObject.<index> 指令
    // 格式1: bufferObject.<index> = <size> [name]
    // 格式2: bufferObject.<index> = <size> <isRelative> <scaleX> <scaleY>
    // 示例1: bufferObject.0 = 4096 MySSBO
    // 示例2: bufferObject.1 = 1024 true 1.0 1.0
    HandlePassDirective("bufferObject.", key, value, [this, &value](const std::string& index)
    {
        auto parts = SplitWhitespace(value);
        if (parts.empty())
        {
            return;
        }

        try
        {
            int bufferIndex = std::stoi(index);
            if (bufferIndex > 8)
            {
                // SSBO索引超出范围（0-8）
                return;
            }

            long size = std::stol(parts[0]);
            if (size < 1)
            {
                // 无效size，忽略
                return;
            }

            if (parts.size() <= 2)
            {
                // 简单格式: size [name]
                std::string name             = (parts.size() > 1) ? parts[1] : "";
                m_bufferObjects[bufferIndex] = ShaderStorageInfo{size, false, 0.0f, 0.0f, name};
            }
            else
            {
                // 完整格式: size isRelative scaleX scaleY
                bool  isRelative             = (parts[1] == "true");
                float scaleX                 = std::stof(parts[2]);
                float scaleY                 = std::stof(parts[3]);
                m_bufferObjects[bufferIndex] = ShaderStorageInfo{size, isRelative, scaleX, scaleY, ""};
            }
        }
        catch (...)
        {
            // 解析失败，忽略
        }
    });

    // ========================================================================
    // 5. 复杂指令（sliders, screen, profile, flip 等）
    // ========================================================================

    // sliders 指令
    // 格式: sliders = <option1> <option2> ...
    // 示例: sliders = SHADOW_QUALITY SHADOW_DISTANCE
    if (key == "sliders")
    {
        m_sliderOptions = SplitWhitespace(value);
    }

    // screen 指令（主屏幕选项）
    // 格式: screen = <option1> <option2> ...
    if (key == "screen")
    {
        m_mainScreenOptions = SplitWhitespace(value);
    }

    // screen.<subscreen> 指令（子屏幕选项）
    // 格式: screen.<subscreen> = <option1> <option2> ...
    // 示例: screen.SHADOWS = SHADOW_QUALITY SHADOW_DISTANCE
    if (key.rfind("screen.", 0) == 0 && key.find(".columns") == std::string::npos)
    {
        std::string subscreen         = key.substr(7);
        m_subScreenOptions[subscreen] = SplitWhitespace(value);
    }

    // screen.columns 指令（主屏幕列数）
    // 格式: screen.columns = <count>
    if (key == "screen.columns")
    {
        try
        {
            m_mainScreenColumnCount = std::stoi(value);
        }
        catch (...)
        {
            // 解析失败，忽略
        }
    }

    // screen.<subscreen>.columns 指令（子屏幕列数）
    // 格式: screen.<subscreen>.columns = <count>
    // 示例: screen.SHADOWS.columns = 2
    if (key.rfind("screen.", 0) == 0 && key.rfind(".columns") == key.length() - 8)
    {
        std::string subscreen = key.substr(7, key.length() - 15);
        try
        {
            m_subScreenColumnCount[subscreen] = std::stoi(value);
        }
        catch (...)
        {
            // 解析失败，忽略
        }
    }

    // profile.<name> 指令（配置文件）
    // 格式: profile.<name> = <option1> <option2> ...
    // 示例: profile.LOW = SHADOW_QUALITY=0 SHADOW_DISTANCE=64
    if (key.rfind("profile.", 0) == 0)
    {
        std::string profileName = key.substr(8);
        m_profiles[profileName] = SplitWhitespace(value);
    }

    // flip.<pass>.<buffer> 指令（纹理翻转）
    // 格式: flip.<pass>.<buffer> = true | false
    // 示例: flip.composite.colortex0 = true
    if (key.rfind("flip.", 0) == 0)
    {
        std::string rest   = key.substr(5);
        size_t      dotPos = rest.find('.');
        if (dotPos != std::string::npos)
        {
            std::string pass   = rest.substr(0, dotPos);
            std::string buffer = rest.substr(dotPos + 1);

            bool shouldFlip               = (value == "true" || value == "1");
            m_explicitFlips[pass][buffer] = shouldFlip;
        }
    }

    // iris.features.required 指令（必需功能标志）
    // 格式: iris.features.required = <flag1> <flag2> ...
    // 示例: iris.features.required = PER_BUFFER_BLENDING SSBO
    if (key == "iris.features.required")
    {
        m_requiredFeatureFlags = SplitWhitespace(value);
    }

    // iris.features.optional 指令（可选功能标志）
    // 格式: iris.features.optional = <flag1> <flag2> ...
    if (key == "iris.features.optional")
    {
        m_optionalFeatureFlags = SplitWhitespace(value);
    }

    // program.<program>.enabled 指令（条件程序启用）
    // 格式: program.<program>.enabled = <condition>
    // 示例: program.composite1.enabled = SHADOW_QUALITY > 0
    if (key.rfind("program.", 0) == 0 && key.rfind(".enabled") == key.length() - 8)
    {
        std::string program                     = key.substr(8, key.length() - 16);
        m_conditionallyEnabledPrograms[program] = value;
    }

    // ========================================================================
    // 6. Custom Texture Directives
    // ========================================================================

    try
    {
        HandleTextureStageDirective(key, value);
    }
    catch (const TextureDirectiveParseException& e)
    {
        ERROR_RECOVERABLE(e.what());
    }

    try
    {
        HandleCustomTextureDirective(key, value);
    }
    catch (const TextureDirectiveParseException& e)
    {
        ERROR_RECOVERABLE(e.what());
    }
}

// ============================================================================
// Helper 方法
// ============================================================================

/**
 * @brief 处理 Boolean Directive
 * @details 如果 key 匹配 expectedKey，则解析 value 并设置 target
 *
 * 教学要点:
 * - 避免重复代码（~40个boolean配置）
 * - 通过引用传递目标成员变量
 */
void ShaderProperties::HandleBooleanDirective(const std::string& key,
                                              const std::string& value,
                                              const std::string& expectedKey,
                                              OptionalBoolean&   target)
{
    if (key == expectedKey)
    {
        target = ParseBooleanValue(value);
    }
}

/**
 * @brief 处理 Int Directive
 * @details 如果 key 匹配 expectedKey，则解析 value 为整数并设置 target
 *
 * @return true 匹配并处理, false 不匹配
 *
 * 教学要点:
 * - 返回bool用于早期返回（避免后续无效匹配）
 */
bool ShaderProperties::HandleIntDirective(const std::string& key,
                                          const std::string& value,
                                          const std::string& expectedKey,
                                          int&               target)
{
    if (key == expectedKey)
    {
        try
        {
            target = std::stoi(value);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
    return false;
}

/**
 * @brief 处理 Pass Directive (例如: scale.<pass>, alphaTest.<pass>)
 * @param prefix 前缀（例如: "scale."）
 * @param key 键名
 * @param value 值
 * @param handler 处理函数（参数: pass 名称）
 *
 * 教学要点:
 * - Lambda表达式作为回调函数
 * - 提取pass名称，委托给handler处理具体解析
 */
void ShaderProperties::HandlePassDirective(const std::string&                      prefix,
                                           const std::string&                      key,
                                           const std::string&                      value,
                                           std::function<void(const std::string&)> handler)
{
    UNUSED(value)
    if (key.rfind(prefix, 0) == 0)
    {
        // 检查前缀
        std::string pass = key.substr(prefix.length());
        handler(pass);
    }
}

// ============================================================================
// 静态工具方法
// ============================================================================

/**
 * @brief 从字符串解析 Boolean 值
 * @param value 字符串值 ("true", "false", "on", "off", "yes", "no", "1", "0")
 * @return OptionalBoolean 结果
 *
 * 教学要点:
 * - 大小写不敏感（使用lambda避免std::transform警告）
 * - 支持多种格式（对齐Java Properties约定）
 */
OptionalBoolean ShaderProperties::ParseBooleanValue(const std::string& value)
{
    std::string lowerValue = value;
    // 使用lambda包装避免C4244警告
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lowerValue == "true" || lowerValue == "on" || lowerValue == "yes" || lowerValue == "1")
    {
        return OptionalBoolean::ENABLED;
    }
    else if (lowerValue == "false" || lowerValue == "off" || lowerValue == "no" || lowerValue == "0")
    {
        return OptionalBoolean::DISABLED;
    }
    else
    {
        return OptionalBoolean::DEFAULT;
    }
}

/**
 * @brief 从字符串解析 AlphaTest.Function
 * @param value 字符串值 (例如: "GREATER", "ALWAYS")
 * @return Function 枚举值
 *
 * 教学要点:
 * - 大小写不敏感（转为大写）
 * - 使用lambda包装避免C4244警告
 */
AlphaTest::Function ShaderProperties::ParseAlphaTestFunction(const std::string& value)
{
    std::string upperValue = value;
    // 使用lambda包装避免C4244警告
    std::transform(upperValue.begin(), upperValue.end(), upperValue.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upperValue == "NEVER") return AlphaTest::Function::NEVER;
    if (upperValue == "LESS") return AlphaTest::Function::LESS;
    if (upperValue == "EQUAL") return AlphaTest::Function::EQUAL;
    if (upperValue == "LEQUAL") return AlphaTest::Function::LEQUAL;
    if (upperValue == "GREATER") return AlphaTest::Function::GREATER;
    if (upperValue == "NOTEQUAL") return AlphaTest::Function::NOTEQUAL;
    if (upperValue == "GEQUAL") return AlphaTest::Function::GEQUAL;
    if (upperValue == "ALWAYS") return AlphaTest::Function::ALWAYS;

    return AlphaTest::Function::ALWAYS; // 默认值
}

/**
 * @brief 从字符串解析 BlendModeOverride.Function
 * @param value 字符串值 (例如: "SRC_ALPHA", "ONE_MINUS_SRC_ALPHA")
 * @return Function 枚举值
 *
 * 教学要点:
 * - 完整实现15个枚举值
 * - 支持"OFF"特殊值（禁用混合）
 * - 使用lambda包装避免C4244警告
 */
BlendModeOverride::Function ShaderProperties::ParseBlendModeFunction(const std::string& value)
{
    std::string upperValue = value;
    // 使用lambda包装避免C4244警告
    std::transform(upperValue.begin(), upperValue.end(), upperValue.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    if (upperValue == "ZERO") return BlendModeOverride::Function::ZERO;
    if (upperValue == "ONE") return BlendModeOverride::Function::ONE;
    if (upperValue == "SRC_COLOR") return BlendModeOverride::Function::SRC_COLOR;
    if (upperValue == "ONE_MINUS_SRC_COLOR") return BlendModeOverride::Function::ONE_MINUS_SRC_COLOR;
    if (upperValue == "DST_COLOR") return BlendModeOverride::Function::DST_COLOR;
    if (upperValue == "ONE_MINUS_DST_COLOR") return BlendModeOverride::Function::ONE_MINUS_DST_COLOR;
    if (upperValue == "SRC_ALPHA") return BlendModeOverride::Function::SRC_ALPHA;
    if (upperValue == "ONE_MINUS_SRC_ALPHA") return BlendModeOverride::Function::ONE_MINUS_SRC_ALPHA;
    if (upperValue == "DST_ALPHA") return BlendModeOverride::Function::DST_ALPHA;
    if (upperValue == "ONE_MINUS_DST_ALPHA") return BlendModeOverride::Function::ONE_MINUS_DST_ALPHA;
    if (upperValue == "CONSTANT_COLOR") return BlendModeOverride::Function::CONSTANT_COLOR;
    if (upperValue == "ONE_MINUS_CONSTANT_COLOR") return BlendModeOverride::Function::ONE_MINUS_CONSTANT_COLOR;
    if (upperValue == "CONSTANT_ALPHA") return BlendModeOverride::Function::CONSTANT_ALPHA;
    if (upperValue == "ONE_MINUS_CONSTANT_ALPHA") return BlendModeOverride::Function::ONE_MINUS_CONSTANT_ALPHA;
    if (upperValue == "SRC_ALPHA_SATURATE") return BlendModeOverride::Function::SRC_ALPHA_SATURATE;
    if (upperValue == "OFF") return BlendModeOverride::Function::OFF;

    return BlendModeOverride::Function::OFF; // 默认值
}

/**
 * @brief 分割空格分隔的列表
 * @param value 字符串值 (例如: "option1 option2 option3")
 * @return 字符串列表
 *
 * 教学要点:
 * - 使用std::istringstream自动处理多个空格/tab
 * - operator>> 自动跳过空白符
 */
std::vector<std::string> ShaderProperties::SplitWhitespace(const std::string& value)
{
    std::vector<std::string> result;
    std::istringstream       stream(value);
    std::string              token;

    while (stream >> token)
    {
        result.push_back(token);
    }

    return result;
}

// ============================================================================
// Custom Texture Directive Handlers
// ============================================================================

//-----------------------------------------------------------------------------------------------
// Handles "texture.<stage>.<textureSlot>=<path>" directives.
// Skips "texture.noise" which is handled separately as noiseTextureResolution.
// Validates stage name against known Iris pipeline stages and slot range 0-15.
// Throws TextureDirectiveParseException on invalid format.
//-----------------------------------------------------------------------------------------------
void ShaderProperties::HandleTextureStageDirective(const std::string& key, const std::string& value)
{
    const std::string prefix = "texture.";
    if (key.rfind(prefix, 0) != 0)
    {
        return;
    }

    // "texture.noise" is handled separately as noiseTextureResolution
    if (key == "texture.noise")
    {
        return;
    }

    // Extract remainder after "texture." (e.g. "composite.5")
    std::string remainder = key.substr(prefix.length());

    // Find the last dot to split "<stage>.<textureSlot>"
    size_t lastDot = remainder.rfind('.');
    if (lastDot == std::string::npos)
    {
        throw TextureDirectiveParseException(
            "Invalid texture directive format (missing textureSlot): " + key);
    }

    std::string stage   = remainder.substr(0, lastDot);
    std::string slotStr = remainder.substr(lastDot + 1);

    if (stage.empty())
    {
        throw TextureDirectiveParseException(
            "Empty stage name in texture directive: " + key);
    }

    if (!IsValidStage(stage))
    {
        throw TextureDirectiveParseException(
            "Invalid stage name in texture directive: " + stage);
    }

    int textureSlot = ParseTextureSlot(slotStr);

    TextureDeclaration decl = ParseTexturePath(value);

    StageTextureBinding binding;
    binding.stage       = stage;
    binding.textureSlot = textureSlot;
    binding.texture     = std::move(decl);

    m_customTextureData.stageBindings.push_back(std::move(binding));
}

//-----------------------------------------------------------------------------------------------
// Handles "customTexture.<name>=<path>" directives.
// Slot assignment is deferred to ShaderBundle (textureSlot = -1 here).
// Throws TextureDirectiveParseException on empty name or path.
//-----------------------------------------------------------------------------------------------
void ShaderProperties::HandleCustomTextureDirective(const std::string& key, const std::string& value)
{
    const std::string prefix = "customTexture.";
    if (key.rfind(prefix, 0) != 0)
    {
        return;
    }

    std::string name = key.substr(prefix.length());
    if (name.empty())
    {
        throw TextureDirectiveParseException(
            "Empty name in customTexture directive: " + key);
    }

    TextureDeclaration decl = ParseTexturePath(value);

    CustomTextureBinding binding;
    binding.name        = name;
    binding.textureSlot = -1; // Assigned later by ShaderBundle
    binding.texture     = std::move(decl);

    m_customTextureData.customBindings.push_back(std::move(binding));
}

//-----------------------------------------------------------------------------------------------
// Parses the texture path value from a directive.
// Strips leading forward/back slashes. Throws on empty result.
//-----------------------------------------------------------------------------------------------
TextureDeclaration ShaderProperties::ParseTexturePath(const std::string& rawValue)
{
    std::string path = rawValue;

    // Strip leading slashes (forward and backward)
    while (!path.empty() && (path[0] == '/' || path[0] == '\\'))
    {
        path = path.substr(1);
    }

    if (path.empty())
    {
        throw TextureDirectiveParseException(
            "Empty texture path after stripping leading slashes");
    }

    TextureDeclaration decl;
    decl.path = path;
    return decl;
}

//-----------------------------------------------------------------------------------------------
// Validates a stage name against known Iris pipeline stage prefixes.
// Accepts: composite[N], deferred[N], prepare[N], gbuffers_*, shadow[N], final
//-----------------------------------------------------------------------------------------------
bool ShaderProperties::IsValidStage(const std::string& stage)
{
    static const std::vector<std::string> validPrefixes = {
        "composite",
        "deferred",
        "prepare",
        "gbuffers_",
        "shadow",
        "final"
    };

    for (const auto& pfx : validPrefixes)
    {
        if (stage.rfind(pfx, 0) == 0)
        {
            return true;
        }
    }

    return false;
}

//-----------------------------------------------------------------------------------------------
// Parses a texture slot string to int and validates range 0-15.
// Throws TextureDirectiveParseException on non-numeric or out-of-range values.
//-----------------------------------------------------------------------------------------------
int ShaderProperties::ParseTextureSlot(const std::string& slotStr)
{
    int slot = -1;
    try
    {
        slot = std::stoi(slotStr);
    }
    catch (...)
    {
        throw TextureDirectiveParseException(
            "Invalid texture slot (not a number): " + slotStr);
    }

    if (slot < 0 || slot > 15)
    {
        throw TextureDirectiveParseException(
            "Texture slot out of range 0-15: " + std::to_string(slot));
    }

    return slot;
}

// ============================================================================
// Getter 方法实现
// ============================================================================

/**
 * @brief 获取粒子渲染设置
 * @return ParticleRenderingSettings 枚举值
 */
ParticleRenderingSettings ShaderProperties::GetParticleRenderingSettings() const
{
    return m_particleRenderingSettings;
}

/**
 * @brief 获取所有 AlphaTest 覆盖配置
 * @return pass -> AlphaTest 映射
 */
const std::unordered_map<std::string, AlphaTest>& ShaderProperties::GetAlphaTestOverrides() const
{
    return m_alphaTestOverrides;
}

/**
 * @brief 获取所有视口缩放覆盖配置
 * @return pass -> ViewportData 映射
 */
const std::unordered_map<std::string, ViewportData>& ShaderProperties::GetViewportScaleOverrides() const
{
    return m_viewportScaleOverrides;
}

/**
 * @brief 获取所有纹理缩放覆盖配置
 * @return pass -> TextureScaleOverride 映射
 */
const std::unordered_map<std::string, TextureScaleOverride>& ShaderProperties::GetTextureScaleOverrides() const
{
    return m_textureScaleOverrides;
}

/**
 * @brief 获取所有混合模式覆盖配置
 * @return pass -> BlendModeOverride 映射
 */
const std::unordered_map<std::string, BlendModeOverride>& ShaderProperties::GetBlendModeOverrides() const
{
    return m_blendModeOverrides;
}

/**
 * @brief 获取所有间接绘制指针配置
 * @return pass -> IndirectPointer 映射
 */
const std::unordered_map<std::string, IndirectPointer>& ShaderProperties::GetIndirectPointers() const
{
    return m_indirectPointers;
}

/**
 * @brief 获取所有缓冲区混合覆盖配置
 * @return pass -> [BufferBlendInformation] 映射
 */
const std::unordered_map<std::string, std::vector<BufferBlendInformation>>& ShaderProperties::GetBufferBlendOverrides() const
{
    return m_bufferBlendOverrides;
}

/**
 * @brief 获取所有 Shader Storage Buffer Object 配置
 * @return index -> ShaderStorageInfo 映射
 */
const std::unordered_map<int, ShaderStorageInfo>& ShaderProperties::GetBufferObjects() const
{
    return m_bufferObjects;
}

/**
 * @brief 获取所有 Slider 选项
 * @return 选项列表
 */
const std::vector<std::string>& ShaderProperties::GetSliderOptions() const
{
    return m_sliderOptions;
}

/**
 * @brief 获取所有 Profile 配置
 * @return profile -> [option] 映射
 */
const std::unordered_map<std::string, std::vector<std::string>>& ShaderProperties::GetProfiles() const
{
    return m_profiles;
}

/**
 * @brief 获取主屏幕选项（可选）
 * @return 选项列表（如果未设置返回 nullopt）
 */
std::optional<std::vector<std::string>> ShaderProperties::GetMainScreenOptions() const
{
    if (m_mainScreenOptions.empty())
    {
        return std::nullopt;
    }
    return m_mainScreenOptions;
}

/**
 * @brief 获取所有子屏幕选项
 * @return subscreen -> [option] 映射
 */
const std::unordered_map<std::string, std::vector<std::string>>& ShaderProperties::GetSubScreenOptions() const
{
    return m_subScreenOptions;
}

/**
 * @brief 获取主屏幕列数（可选）
 * @return 列数（如果未设置返回 nullopt）
 */
std::optional<int> ShaderProperties::GetMainScreenColumnCount() const
{
    if (m_mainScreenColumnCount == -1)
    {
        return std::nullopt;
    }
    return m_mainScreenColumnCount;
}

/**
 * @brief 获取所有子屏幕列数
 * @return subscreen -> column_count 映射
 */
const std::unordered_map<std::string, int>& ShaderProperties::GetSubScreenColumnCount() const
{
    return m_subScreenColumnCount;
}

/**
 * @brief 获取必需功能标志列表
 * @return 功能标志列表
 */
const std::vector<std::string>& ShaderProperties::GetRequiredFeatureFlags() const
{
    return m_requiredFeatureFlags;
}

/**
 * @brief 获取可选功能标志列表
 * @return 功能标志列表
 */
const std::vector<std::string>& ShaderProperties::GetOptionalFeatureFlags() const
{
    return m_optionalFeatureFlags;
}

/**
 * @brief 获取所有显式翻转配置
 * @return pass -> (buffer -> bool) 映射
 */
const std::unordered_map<std::string, std::unordered_map<std::string, bool>>& ShaderProperties::GetExplicitFlips() const
{
    return m_explicitFlips;
}

/**
 * @brief 获取所有条件启用程序配置
 * @return program -> condition 映射
 */
const std::unordered_map<std::string, std::string>& ShaderProperties::GetConditionallyEnabledPrograms() const
{
    return m_conditionallyEnabledPrograms;
}

// ============================================================================
// BlendModeOverride 静态方法
// ============================================================================

/**
 * @brief 创建禁用混合的 BlendModeOverride
 * @return BlendModeOverride(OFF, OFF, OFF, OFF)
 *
 * 教学要点:
 * - 工厂方法模式
 * - 语义化接口（Off() 比直接构造更清晰）
 */
BlendModeOverride BlendModeOverride::Off()
{
    return BlendModeOverride{
        Function::OFF,
        Function::OFF,
        Function::OFF,
        Function::OFF
    };
}
