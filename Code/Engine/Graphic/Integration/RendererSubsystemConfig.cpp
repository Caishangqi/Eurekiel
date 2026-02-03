/**
 * @file RendererSubsystemConfig.cpp
 * @brief RendererSubsystem配置系统实现
 *
 * @details
 * Milestone 3.0 配置系统重构 (2025-10-21):
 * - 实现ParseFromYaml()静态解析方法
 * - 实现GetDefault()默认配置方法
 * - 参数验证和日志记录
 *
 * 教学要点:
 * 1. YAML解析模式: TryLoadFromFile() → GetInt/GetString → 参数验证
 * 2. 错误处理: 文件不存在、参数超范围、无效值等
 * 3. 日志记录: 使用LogInfo/LogWarn记录配置加载过程
 *
 * @note 架构参考: Engine/Core/Schedule/ScheduleSubsystem.cpp (ScheduleConfig实现)
 */

#include "RendererSubsystemConfig.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Target/RenderTargetProviderCommon.hpp"
#include "Engine/Graphic/Shader/Program/Parsing/DXGIFormatParser.hpp"

using namespace enigma::core;

namespace enigma::graphic
{
    // ============================================================================
    // [NEW] Helper functions for YAML RT config parsing
    // ============================================================================
    namespace
    {
        // Parse single RT config from YAML node
        RenderTargetConfig ParseSingleRTConfig(
            const YAML::Node&         node,
            const RenderTargetConfig& defaultConfig,
            const std::string&        rtName,
            bool                      isDepthFormat)
        {
            RenderTargetConfig config = defaultConfig;
            config.name               = rtName;

            // Parse format
            if (node["format"])
            {
                std::string formatStr = node["format"].as<std::string>("");
                auto        formatOpt = DXGIFormatParser::Parse(formatStr);
                if (formatOpt.has_value())
                {
                    config.format = formatOpt.value();
                }
                else
                {
                    LogWarn("RendererSubsystemConfig",
                            "Invalid format '%s' for %s, using default",
                            formatStr.c_str(), rtName.c_str());
                }
            }

            // Parse clearValue
            if (node["clearValue"])
            {
                if (isDepthFormat)
                {
                    // Depth clear value (single float)
                    float depthVal    = node["clearValue"].as<float>(1.0f);
                    config.clearValue = ClearValue::Depth(depthVal, 0);
                }
                else if (node["clearValue"].IsSequence() && node["clearValue"].size() >= 3)
                {
                    // Color clear value (array of 3-4 floats)
                    float r           = node["clearValue"][0].as<float>(0.0f);
                    float g           = node["clearValue"][1].as<float>(0.0f);
                    float b           = node["clearValue"][2].as<float>(0.0f);
                    float a           = (node["clearValue"].size() >= 4) ? node["clearValue"][3].as<float>(1.0f) : 1.0f;
                    config.clearValue = ClearValue::Color(r, g, b, a);
                }
            }

            // Parse enableClear -> loadAction
            if (node["enableClear"])
            {
                bool enableClear  = node["enableClear"].as<bool>(true);
                config.loadAction = enableClear ? LoadAction::Clear : LoadAction::Load;
            }

            // Parse enableFlipper (color targets only)
            if (!isDepthFormat && node["enableFlipper"])
            {
                config.enableFlipper = node["enableFlipper"].as<bool>(true);
            }

            // Parse dimensions - absolute size (priority) or scale (fallback)
            // Priority: width/height > 0 → use absolute; otherwise use scale * renderResolution
            if (node["width"])
            {
                config.width = node["width"].as<int>(0);
            }
            if (node["height"])
            {
                config.height = node["height"].as<int>(0);
            }
            if (node["widthScale"])
            {
                config.widthScale = node["widthScale"].as<float>(1.0f);
            }
            if (node["heightScale"])
            {
                config.heightScale = node["heightScale"].as<float>(1.0f);
            }

            return config;
        }

        // Parse RenderTargetTypeConfig from YAML section
        void ParseRTTypeConfigFromYaml(
            const YAML::Node&                                sectionNode,
            RendererSubsystemConfig::RenderTargetTypeConfig& rtTypeConfig,
            const std::string&                               rtPrefix,
            int                                              maxCount,
            bool                                             isDepthFormat)
        {
            if (!sectionNode || !sectionNode.IsMap())
                return;

            // Parse defaultConfig
            if (sectionNode["defaultConfig"])
            {
                rtTypeConfig.defaultConfig = ParseSingleRTConfig(
                    sectionNode["defaultConfig"],
                    rtTypeConfig.defaultConfig,
                    rtPrefix + "_default",
                    isDepthFormat
                );
            }

            // Parse index-specific configs
            if (sectionNode["configs"] && sectionNode["configs"].IsMap())
            {
                for (auto it = sectionNode["configs"].begin(); it != sectionNode["configs"].end(); ++it)
                {
                    int index = -1;
                    try
                    {
                        index = it->first.as<int>();
                    }
                    catch (...)
                    {
                        continue; // Skip invalid index
                    }

                    if (index < 0 || index >= maxCount)
                    {
                        LogWarn("RendererSubsystemConfig",
                                "%s index %d out of range [0, %d), skipping",
                                rtPrefix.c_str(), index, maxCount);
                        continue;
                    }

                    std::string rtName          = rtPrefix + std::to_string(index);
                    rtTypeConfig.configs[index] = ParseSingleRTConfig(
                        it->second,
                        rtTypeConfig.defaultConfig,
                        rtName,
                        isDepthFormat
                    );
                }
            }
        }

        // Parse all rendertargets section
        void ParseRenderTargetsConfig(const YamlConfiguration& yaml, RendererSubsystemConfig& config)
        {
            const YAML::Node& root = yaml.GetNode();
            if (!root["rendertargets"])
                return;

            const YAML::Node& rtNode = root["rendertargets"];

            // Parse colortexture section
            if (rtNode["colortexture"])
            {
                ParseRTTypeConfigFromYaml(
                    rtNode["colortexture"],
                    config.colorTexConfig,
                    "colortex",
                    MAX_COLOR_TEXTURES,
                    false // isDepthFormat
                );
            }

            // Parse depthtexture section
            if (rtNode["depthtexture"])
            {
                ParseRTTypeConfigFromYaml(
                    rtNode["depthtexture"],
                    config.depthTexConfig,
                    "depthtex",
                    MAX_DEPTH_TEXTURES,
                    true // isDepthFormat
                );
            }

            // Parse shadowcolor section
            if (rtNode["shadowcolor"])
            {
                ParseRTTypeConfigFromYaml(
                    rtNode["shadowcolor"],
                    config.shadowColorConfig,
                    "shadowcolor",
                    MAX_SHADOW_COLORS,
                    false // isDepthFormat
                );
            }

            // Parse shadowtexture section
            if (rtNode["shadowtexture"])
            {
                ParseRTTypeConfigFromYaml(
                    rtNode["shadowtexture"],
                    config.shadowTexConfig,
                    "shadowtex",
                    MAX_SHADOW_TEXTURES,
                    true // isDepthFormat
                );
            }

            LogInfo("RendererSubsystemConfig",
                    "Parsed rendertargets config: colortex=%d, depthtex=%d, shadowcolor=%d, shadowtex=%d custom configs",
                    static_cast<int>(config.colorTexConfig.configs.size()),
                    static_cast<int>(config.depthTexConfig.configs.size()),
                    static_cast<int>(config.shadowColorConfig.configs.size()),
                    static_cast<int>(config.shadowTexConfig.configs.size()));
        }
    } // anonymous namespace

    //-----------------------------------------------------------------------------------------------
    // ParseFromYaml: 从YAML配置文件解析RendererSubsystem配置
    // Milestone 3.0: 合并Configuration配置项，扩展为14个配置参数
    //-----------------------------------------------------------------------------------------------
    std::optional<RendererSubsystemConfig> RendererSubsystemConfig::ParseFromYaml(const std::string& yamlPath)
    {
        // 步骤1: 尝试加载YAML文件
        auto yamlOpt = YamlConfiguration::TryLoadFromFile(yamlPath);

        if (!yamlOpt.has_value())
        {
            // 文件不存在或加载失败，记录警告并返回nullopt
            LogWarn("RendererSubsystemConfig",
                    "Failed to load config from: {}. Using default config.",
                    yamlPath.c_str());
            return std::nullopt;
        }

        // 步骤2: 创建配置对象（使用默认值初始化）
        RendererSubsystemConfig result = GetDefault();

        // 步骤3: 解析基础渲染配置
        // YAML路径: rendering.*
        result.renderWidth       = yamlOpt->GetInt("rendering.width", 1920);
        result.renderHeight      = yamlOpt->GetInt("rendering.height", 1080);
        result.maxFramesInFlight = yamlOpt->GetInt("rendering.maxFramesInFlight", 3);

        // 参数验证: 分辨率必须大于0
        if (result.renderWidth <= 0 || result.renderHeight <= 0)
        {
            LogWarn("RendererSubsystemConfig",
                    "Invalid resolution {}x{}. Using default 1920x1080.",
                    result.renderWidth, result.renderHeight);
            result.renderWidth  = 1920;
            result.renderHeight = 1080;
        }

        // 参数验证: maxFramesInFlight必须在 [2, 3] 范围内
        if (result.maxFramesInFlight < 2 || result.maxFramesInFlight > 3)
        {
            LogWarn("RendererSubsystemConfig",
                    "rendering.maxFramesInFlight {} out of range [2, 3]. Using default 3.",
                    result.maxFramesInFlight);
            result.maxFramesInFlight = 3;
        }

        // 步骤4: 解析调试配置
        // YAML路径: debug.*
        result.enableDebugLayer        = yamlOpt->GetBoolean("debug.enableDebugLayer", true);
        result.enableGPUValidation     = yamlOpt->GetBoolean("debug.enableGPUValidation", true);
        result.enableBindlessResources = yamlOpt->GetBoolean("debug.enableBindlessResources", true);

        // 步骤5.5: 解析Shader编译配置
        // YAML路径: shader.entryPoint
        result.shaderEntryPoint = yamlOpt->GetString("shader.entryPoint", "main");

        // 参数验证: 入口点名称不能为空
        if (result.shaderEntryPoint.empty())
        {
            LogWarn("RendererSubsystemConfig",
                    "shader.entryPoint is empty. Using default 'main'.");
            result.shaderEntryPoint = "main";
        }

        LogInfo("RendererSubsystemConfig", "Shader entry point: %s", result.shaderEntryPoint.c_str());

        // [NEW] Step 8: Parse rendertargets configuration
        ParseRenderTargetsConfig(*yamlOpt, result);

        return result;
    }

    //-----------------------------------------------------------------------------------------------
    // GetDefault: 获取默认配置
    // 提供安全的默认值，确保系统在配置文件缺失时仍能正常运行
    //-----------------------------------------------------------------------------------------------
    RendererSubsystemConfig RendererSubsystemConfig::GetDefault()
    {
        RendererSubsystemConfig config;

        // 默认值已在struct定义中设置:
        // - gbufferColorTexCount = 8 (平衡内存和功能)
        // - renderWidth = 1920 (Full HD)
        // - renderHeight = 1080 (Full HD)

        // [NEW] Initialize default RT configs
        // ColorTex default: R8G8B8A8_UNORM, clear to black
        config.colorTexConfig.defaultConfig = RenderTargetConfig::ColorTargetWithScale(
            "colortex_default", 1.0f, 1.0f,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            true, // enableFlipper
            LoadAction::Clear,
            ClearValue::Color(0.0f, 0.0f, 0.0f, 1.0f)
        );

        // DepthTex default: D24_UNORM_S8_UINT, clear to 1.0
        config.depthTexConfig.defaultConfig = RenderTargetConfig::DepthTargetWithScale(
            "depthtex_default", 1.0f, 1.0f,
            DXGI_FORMAT_D24_UNORM_S8_UINT,
            LoadAction::Clear,
            ClearValue::Depth(1.0f, 0)
        );

        // ShadowColor default: R8G8B8A8_UNORM, clear to white
        // [Iris Ref] PackShadowDirectives.java:399-405 - clearColor = new Vector4f(1.0F)
        // White = no shadow color modulation, black would incorrectly absorb all light
        config.shadowColorConfig.defaultConfig = RenderTargetConfig::ColorTargetWithScale(
            "shadowcolor_default", 1.0f, 1.0f,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            true, // enableFlipper
            LoadAction::Clear,
            ClearValue::Color(1.0f, 1.0f, 1.0f, 1.0f)
        );

        // ShadowTex default: D32_FLOAT, clear to 1.0
        config.shadowTexConfig.defaultConfig = RenderTargetConfig::DepthTargetWithScale(
            "shadowtex_default", 1.0f, 1.0f,
            DXGI_FORMAT_D32_FLOAT,
            LoadAction::Clear,
            ClearValue::Depth(1.0f, 0)
        );

        return config;
    }

    // ============================================================================
    // [REFACTOR] RT Config Accessor Methods - Calculate actual dimensions from scale
    // ============================================================================

    std::vector<RenderTargetConfig> RendererSubsystemConfig::GetColorTexConfigs() const
    {
        std::vector<RenderTargetConfig> result;
        result.reserve(MAX_COLOR_TEXTURES);

        for (int i = 0; i < MAX_COLOR_TEXTURES; ++i)
        {
            RenderTargetConfig cfg = colorTexConfig.GetConfig(i);
            cfg.name               = "colortex" + std::to_string(i);

            // [FIX] Calculate actual dimensions from scale if width/height are 0
            if (cfg.width <= 0 || cfg.height <= 0)
            {
                cfg.width  = static_cast<int>(static_cast<float>(renderWidth) * cfg.widthScale);
                cfg.height = static_cast<int>(static_cast<float>(renderHeight) * cfg.heightScale);
            }

            result.push_back(cfg);
        }

        return result;
    }

    std::vector<RenderTargetConfig> RendererSubsystemConfig::GetDepthTexConfigs() const
    {
        std::vector<RenderTargetConfig> result;
        result.reserve(MAX_DEPTH_TEXTURES);

        for (int i = 0; i < MAX_DEPTH_TEXTURES; ++i)
        {
            RenderTargetConfig cfg = depthTexConfig.GetConfig(i);
            cfg.name               = "depthtex" + std::to_string(i);

            // [FIX] Calculate actual dimensions from scale if width/height are 0
            if (cfg.width <= 0 || cfg.height <= 0)
            {
                cfg.width  = static_cast<int>(static_cast<float>(renderWidth) * cfg.widthScale);
                cfg.height = static_cast<int>(static_cast<float>(renderHeight) * cfg.heightScale);
            }

            result.push_back(cfg);
        }

        return result;
    }

    std::vector<RenderTargetConfig> RendererSubsystemConfig::GetShadowColorConfigs() const
    {
        std::vector<RenderTargetConfig> result;
        result.reserve(MAX_SHADOW_COLORS);

        for (int i = 0; i < MAX_SHADOW_COLORS; ++i)
        {
            RenderTargetConfig cfg = shadowColorConfig.GetConfig(i);
            cfg.name               = "shadowcolor" + std::to_string(i);

            // [FIX] Calculate actual dimensions from scale if width/height are 0
            if (cfg.width <= 0 || cfg.height <= 0)
            {
                cfg.width  = static_cast<int>(static_cast<float>(renderWidth) * cfg.widthScale);
                cfg.height = static_cast<int>(static_cast<float>(renderHeight) * cfg.heightScale);
            }

            result.push_back(cfg);
        }

        return result;
    }

    std::vector<RenderTargetConfig> RendererSubsystemConfig::GetShadowTexConfigs() const
    {
        std::vector<RenderTargetConfig> result;
        result.reserve(MAX_SHADOW_TEXTURES);

        for (int i = 0; i < MAX_SHADOW_TEXTURES; ++i)
        {
            RenderTargetConfig cfg = shadowTexConfig.GetConfig(i);
            cfg.name               = "shadowtex" + std::to_string(i);

            // [FIX] Calculate actual dimensions from scale if width/height are 0
            if (cfg.width <= 0 || cfg.height <= 0)
            {
                cfg.width  = static_cast<int>(static_cast<float>(renderWidth) * cfg.widthScale);
                cfg.height = static_cast<int>(static_cast<float>(renderHeight) * cfg.heightScale);
            }

            result.push_back(cfg);
        }

        return result;
    }
} // namespace enigma::graphic
