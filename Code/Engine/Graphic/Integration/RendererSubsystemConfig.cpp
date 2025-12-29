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

using namespace enigma::core;

namespace enigma::graphic
{
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

        LogInfo("RendererSubsystemConfig",
                "Shader entry point: %s", result.shaderEntryPoint.c_str());

        // 步骤6: 解析GBuffer配置
        // YAML路径: gbuffer.colorTexCount
        result.gbufferColorTexCount = yamlOpt->GetInt("gbuffer.colorTexCount", 8);

        // 参数验证: colorTexCount必须在 [1, 16] 范围内
        if (result.gbufferColorTexCount < 1 || result.gbufferColorTexCount > 16)
        {
            LogWarn("RendererSubsystemConfig",
                    "gbuffer.colorTexCount %d out of range [1, 16]. Using default 8.",
                    result.gbufferColorTexCount);
            result.gbufferColorTexCount = 8; // 回退到默认值
        }

        // 步骤7: 解析Immediate模式配置
        // YAML路径: immediate.*
        result.enableImmediateMode    = yamlOpt->GetBoolean("immediate.enable", true);
        result.maxCommandsPerPhase    = yamlOpt->GetInt("immediate.maxCommandsPerPhase", 10000);
        result.enablePhaseDetection   = yamlOpt->GetBoolean("immediate.enablePhaseDetection", true);
        result.enableCommandProfiling = yamlOpt->GetBoolean("immediate.enableCommandProfiling", false);

        // 参数验证: maxCommandsPerPhase必须大于0
        if (result.maxCommandsPerPhase <= 0)
        {
            LogWarn("RendererSubsystemConfig",
                    "immediate.maxCommandsPerPhase {} invalid. Using default 10000.",
                    result.maxCommandsPerPhase);
            result.maxCommandsPerPhase = 10000;
        }

        // 步骤8: 注意事项 - 以下配置不从YAML加载（运行时设置）
        // - targetWindow: 运行时通过代码设置
        // - defaultClearColor: 使用默认值Rgba8::DEBUG_GREEN
        // - defaultClearDepth/Stencil: 使用默认值
        // - enableAutoClearColor/Depth: 使用默认值
        // - enableShadowMapping: 未来Milestone实现

        // 步骤9: 记录配置加载成功信息
        LogInfo("RendererSubsystemConfig", "Loaded config from {}: resolution={}x{}, colorTex={}, maxFrames={}, immediateMode={}", yamlPath.c_str(),
                result.renderWidth,
                result.renderHeight,
                result.gbufferColorTexCount,
                result.maxFramesInFlight,
                result.enableImmediateMode);

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

        // 教学要点: 使用成员初始化器 (在.hpp中) 提供默认值
        // 这样GetDefault()只需返回默认构造的对象即可

        return config;
    }
} // namespace enigma::graphic
