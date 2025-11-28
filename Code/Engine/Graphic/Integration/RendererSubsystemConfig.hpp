/**
 * @file RendererSubsystemConfig.hpp
 * @brief RendererSubsystem配置系统 - 遵循"一个子系统一个配置文件"原则
 *
 * @details
 * Milestone 3.0 配置系统重构 (2025-10-21):
 * - 将配置从内联结构体提取为独立Config类
 * - 遵循ScheduleSubsystem的配置模式 (ScheduleConfig)
 * - 统一配置文件：renderer.yml (包含所有RendererSubsystem配置)
 * - 支持YAML解析、默认值和参数验证
 *
 * 教学要点:
 * 1. 配置类设计原则:
 *    - 单一职责: Config类只负责配置数据，不包含业务逻辑
 *    - 静态解析: ParseFromYaml()为静态方法，无需实例化
 *    - 默认值机制: GetDefault()提供安全的默认配置
 *    - 参数验证: 在解析时验证参数范围，记录警告
 *
 * 2. RAII原则:
 *    - Config类是纯数据结构（POD），无需Initialize
 *    - 资源类（如RenderTargetManager）应在构造时完整初始化
 *    - 子系统级别类（RendererSubsystem）可以有Initialize/Startup
 *
 * 3. 配置层次结构:
 *    - renderer.yml (根级配置文件)
 *      ├── gbuffer (GBuffer模块配置)
 *      ├── resolution (分辨率配置)
 *      └── 未来扩展 (vsync, msaa, hdr等)
 *
 * @note 架构参考: Engine/Core/Schedule/ScheduleSubsystem.hpp:69-82 (ScheduleConfig)
 */

#pragma once

#include "Engine/Core/Yaml.hpp"
#include "Engine/Core/Rgba8.hpp"
#include <optional>
#include <cstdint>
#include <string>

// 前向声明
class Window;

namespace enigma::graphic
{
    /**
     * @brief RendererSubsystem配置结构
     *
     * @details
     * Milestone 3.0 配置合并 (2025-10-21):
     * - 合并了旧的RendererSubsystem::Configuration内嵌结构
     * - 统一为RendererSubsystemConfig，实现配置系统一致性
     * - 包含所有RendererSubsystem的可配置参数（共14个）
     *
     * 配置分类:
     * - 基础渲染配置 (分辨率、帧数、Debug)
     * - ShaderPack配置 (用户ShaderPack名称)
     * - 清屏配置 (颜色、深度、自动清屏)
     * - 窗口集成 (目标窗口指针)
     * - GBuffer配置 (colortex数量)
     * - Immediate模式配置 (命令队列、性能分析)
     *
     * 教学要点:
     * 1. 配置合并原则:
     *    - 单一职责: 一个子系统一个配置类
     *    - 避免重复: 删除内联Configuration，统一使用Config类
     *    - 类型一致性: renderWidth/Height保持uint32_t
     *
     * 2. 成员初始化:
     *    - POD结构: 仅数据成员，无复杂逻辑
     *    - 默认值: 使用成员初始化器（C++11特性）
     *    - 前向声明: Window*使用前向声明，Rgba8需要完整定义
     *
     * 3. YAML解析策略:
     *    - 运行时配置: 从YAML加载（如分辨率、Debug标志）
     *    - 编译时配置: 使用默认值（如targetWindow、defaultClearColor）
     *    - 混合配置: 部分从YAML，部分运行时设置
     *
     * @note 架构参考: RendererSubsystem.hpp:72-125 (旧Configuration定义)
     */
    struct RendererSubsystemConfig
    {
        // ==================== 基础渲染配置 ====================

        /**
         * @brief 渲染宽度（像素）
         *
         * 默认值: 1920 (Full HD 1080p)
         *
         * Window Size Data Flow (窗口尺寸数据流) - 2025-10-22修复:
         * ==========================================================
         * 1. AppTest.cpp: g_theWindow (1920x1080)
         *      ↓
         * 2. renderConfig.renderWidth = window->GetClientWidth()
         *      ↓ 默认使用窗口尺寸
         *    config.renderWidth = 1920
         *      ↓ (可选) YAML配置覆盖
         *    if (yaml["resolution"]["width"]) config.renderWidth = yaml["resolution"]["width"]
         *      ↓
         * 3. D3D12RenderSystem::Initialize(config)
         *      ↓ 创建SwapChain
         *    SwapChain(config.renderWidth, config.renderHeight)
         *      ↓
         * 4. RenderTargetManager(width, height, ...)
         *      ↓ 创建RenderTargets
         *    RenderTarget(width, height)
         *
         * 默认行为: RenderTarget尺寸 = SwapChain尺寸 = Window尺寸
         * 可覆盖: 通过YAML配置不同的renderWidth/Height（用于超采样、质量调整）
         *
         * 教学要点:
         * - 数据流清晰: Window → Config → RenderSystem → RenderTarget
         * - 默认值合理: 使用窗口尺寸作为默认值
         * - 可配置性: 允许YAML覆盖（用于渲染质量调整、ShaderPack覆写）
         * - 可选配置: 通常从窗口系统动态获取
         * - 初始化备用: 窗口未创建时的默认分辨率
         * - 动态调整: 支持运行时分辨率变化（OnResize）
         *
         * @note 合并自Configuration::renderWidth
         */
        uint32_t renderWidth = 1920;

        /**
         * @brief 渲染高度（像素）
         *
         * 默认值: 1080 (Full HD 1080p)
         *
         * 教学要点:
         * - 与renderWidth配套使用，共同定义渲染分辨率
         * - 默认值: 使用窗口高度（window->GetClientHeight()）
         * - 可覆盖: YAML配置可以指定不同的高度（如4K渲染）
         *
         * @note 合并自Configuration::renderHeight
         * @see renderWidth 的详细数据流说明
         */
        uint32_t renderHeight = 1080;

        /**
         * @brief 最大飞行帧数
         *
         * 范围: [2-3]
         * - 2: 双缓冲（低延迟，GPU可能等待）
         * - 3: 三缓冲（推荐，平衡延迟和性能）
         *
         * 教学要点:
         * - Frame Pipelining: CPU和GPU并行工作
         * - 输入延迟: 更多飞行帧 = 更高延迟
         * - 性能权衡: 减少GPU等待 vs 增加输入延迟
         *
         * @note 合并自Configuration::maxFramesInFlight
         */
        uint32_t maxFramesInFlight = 3;

        /**
         * @brief 启用DirectX 12调试层
         *
         * 调试层功能:
         * - 参数验证: 检查API调用参数是否有效
         * - 资源跟踪: 检测内存泄漏和资源使用错误
         * - 错误报告: 提供详细的错误消息和堆栈跟踪
         *
         * 性能影响: 显著降低性能（约20-50%），仅用于开发
         *
         * @note 合并自Configuration::enableDebugLayer
         */
        bool enableDebugLayer = true;

        /**
         * @brief 启用GPU验证层
         *
         * GPU验证功能:
         * - Shader验证: 检测Shader错误（越界访问、无效操作）
         * - 资源状态验证: 验证资源状态转换正确性
         * - 同步验证: 检测并发访问问题
         *
         * 性能影响: 极大降低性能（约50-80%），仅用于深度调试
         *
         * @note 合并自Configuration::enableGPUValidation
         */
        bool enableGPUValidation = true;

        /**
         * @brief 启用Bindless资源（SM6.6特性）
         *
         * Bindless优势:
         * - 百万级资源访问: 不受Root Signature限制
         * - 动态索引: 运行时动态选择纹理
         * - 简化管线: 无需频繁切换Root Signature
         *
         * 教学要点:
         * - 硬件要求: DirectX 12.1+ (SM6.6)
         * - 架构设计: D12Buffer/D12Texture内置Bindless索引
         * - 性能提升: 减少绑定开销，提高Draw Call效率
         *
         * @note 合并自Configuration::enableBindlessResources
         */
        bool enableBindlessResources = true;

        // ==================== ShaderPack配置 ====================

        /**
         * @brief 当前ShaderPack名称（空字符串=使用引擎默认）
         *
         * ShaderPack系统（Phase 6.3）:
         * - 空字符串: 使用ENGINE_DEFAULT_SHADERPACK_PATH
         * - "PackName": 使用USER_SHADERPACK_SEARCH_PATH/PackName
         *
         * 示例: "ComplementaryReimagined", "BSL", "Sildurs"
         *
         * 未来增强（Milestone 3.2+）:
         * - 从GameConfig.xml加载（持久化用户选择）
         * - 热重载支持（ReloadShaderPack方法）
         *
         * 架构设计理由:
         * - 用户可配置参数 → 属于Configuration
         * - 固定路径（ENGINE_DEFAULT_PATH） → private constexpr
         * - 参考Iris: IrisConfig::shaderPackName vs Iris::shaderpacksDirectory
         *
         * @note 合并自Configuration::currentShaderPackName
         */
        std::string currentShaderPackName;

        /**
         * @brief 用户ShaderPack搜索路径
         *
         * 默认值: "" (空字符串)
         *
         * 教学要点:
         * - ShaderPack路径管理: 用户可指定自定义ShaderPack目录
         * - 路径解析顺序: 用户包 → 引擎默认包
         */
        std::string shaderPackSearchPath;

        /**
         * @brief 引擎默认ShaderPack路径
         *
         * 默认值: "" (空字符串)
         *
         * 教学要点:
         * - Fallback机制: 当用户ShaderPack缺失时使用引擎默认包
         * - 引擎内置资源: 确保渲染系统始终有可用的ShaderPack
         */
        std::string engineDefaultShaderPackPath;

        // ==================== Shader编译配置 ====================

        /**
         * @brief Shader入口点名称（全局统一）
         * 
         * 默认值: "main" (Iris兼容)
         * 
         * 教学要点:
         * - Iris兼容性: Iris使用 "main" 作为入口点（GLSL标准）
         * - 简化配置: 所有shader阶段使用相同入口点
         * - 用户友好: 符合GLSL/HLSL习惯
         * - 配置驱动: 避免硬编码，提高灵活性
         * 
         * 使用场景:
         * - Iris ShaderPack: 使用 "main" (默认值)
         * - 自定义Shader: 可配置为 "VSMain"/"PSMain" (DirectX风格)
         * - 测试Shader: 可配置为任意名称
         * 
         * 架构设计:
         * - 全局配置: 所有shader阶段使用相同入口点（简化配置）
         * - 可扩展性: 未来可扩展为每个阶段独立配置
         * - 默认值合理: "main" 符合GLSL标准，Iris完全兼容
         * 
         * YAML配置示例:
         * @code{.yaml}
         * shader:
         *   entryPoint: "main"  # Iris兼容（默认）
         *   # entryPoint: "VSMain"  # DirectX风格
         * @endcode
         * 
         * @note 参考Iris: 所有shader使用 "main()" 作为入口点
         * @see ShaderCompilationHelper::GetEntryPoint() - 使用此配置
         */
        std::string shaderEntryPoint = "main";

        // ==================== 清屏配置 ====================

        /**
         * @brief 默认清屏颜色
         *
         * 默认值: Rgba8::DEBUG_GREEN (调试可见性)
         *
         * 教学要点:
         * - 调试可见性: 绿色背景快速识别渲染错误
         * - 生产环境: 改为黑色或场景相关颜色
         *
         * @note 合并自Configuration::defaultClearColor
         */
        Rgba8 defaultClearColor = Rgba8(25, 31, 52);

        /**
         * @brief 默认深度清除值
         *
         * 范围: [0.0-1.0]
         * - 0.0: 最近（Reverse-Z）
         * - 1.0: 最远（标准深度）
         *
         * 教学要点:
         * - 深度缓冲: 控制深度测试初始值
         * - Reverse-Z: 提高深度精度（未来优化）
         *
         * @note 合并自Configuration::defaultClearDepth
         */
        float defaultClearDepth = 1.0f;

        /**
         * @brief 默认模板清除值
         *
         * 范围: [0-255]
         *
         * 教学要点:
         * - 模板缓冲: 用于模板测试和掩码操作
         * - 典型用途: 阴影体积、轮廓渲染
         *
         * @note 合并自Configuration::defaultClearStencil
         */
        uint8_t defaultClearStencil = 0;

        /**
         * @brief 是否在BeginFrame自动执行清屏
         *
         * 教学要点:
         * - 自动化: 简化用户代码，减少错误
         * - 手动控制: 禁用以实现特殊渲染效果（如运动模糊）
         *
         * @note 合并自Configuration::enableAutoClearColor
         */
        bool enableAutoClearColor = true;

        /**
         * @brief 是否在BeginFrame自动清除深度缓冲
         *
         * @note 合并自Configuration::enableAutoClearDepth
         */
        bool enableAutoClearDepth = true;

        /**
         * @brief 是否启用阴影映射（后续Milestone实现）
         *
         * @note 合并自Configuration::enableShadowMapping
         */
        bool enableShadowMapping = false;

        // ==================== 窗口系统集成 ====================

        /**
         * @brief 目标窗口指针，用于SwapChain创建
         *
         * 教学要点:
         * - 运行时设置: 构造函数/Initialize之前设置
         * - 前向声明: 使用前向声明避免循环依赖
         * - 空值检查: 支持Headless模式（无窗口渲染）
         *
         * @note 合并自Configuration::targetWindow
         */
        Window* targetWindow = nullptr;

        // ==================== GBuffer配置 ====================

        /**
         * @brief GBuffer的colortex数量
         *
         * 范围: [1-16]
         * - 最小值: 1个colortex (简化场景)
         * - 推荐值: 8个colortex (平衡内存和功能)
         * - 最大值: 16个colortex (Iris完全兼容)
         *
         * 内存优化示例 (1920x1080, R8G8B8A8格式):
         * - 4个colortex:  约33.2MB  (节省75%，~99.4MB)
         * - 8个colortex:  约66.3MB  (节省50%)
         * - 16个colortex: 约132.6MB (Iris兼容)
         *
         * 教学要点:
         * - 配置驱动: 用户可根据场景复杂度调整内存占用
         * - 性能权衡: 更多colortex支持更复杂的材质系统
         * - Iris兼容: 16个colortex与Iris ShaderPack完全兼容
         */
        int gbufferColorTexCount = 8;

        // ==================== Immediate模式配置 ====================

        /**
         * @brief 启用Immediate模式渲染
         *
         * Immediate模式特性:
         * - 命令缓冲: 收集渲染命令到队列，延迟执行
         * - 阶段检测: 自动检测WorldRenderingPhase
         * - 性能分析: 可选的命令级别性能分析
         *
         * 教学要点:
         * - 延迟渲染: 收集命令 → 排序优化 → 批量执行
         * - 状态管理: 减少状态切换，提高效率
         *
         * @note 合并自Configuration::enableImmediateMode
         */
        bool enableImmediateMode = true;

        /**
         * @brief 每个阶段的最大命令数
         *
         * 范围: [1000-100000]
         * - 1000: 测试模式（减少内存占用）
         * - 10000: 推荐值（平衡内存和性能）
         * - 100000: 复杂场景（大量绘制调用）
         *
         * 教学要点:
         * - 内存权衡: 更大队列 = 更多内存占用
         * - 性能考虑: 队列满时触发Flush，影响性能
         *
         * @note 合并自Configuration::maxCommandsPerPhase
         */
        size_t maxCommandsPerPhase = 10000;

        /**
         * @brief 启用自动Phase检测
         *
         * Phase检测功能:
         * - 自动识别: 根据渲染命令自动识别WorldRenderingPhase
         * - 简化API: 用户无需手动指定Phase
         * - 调试模式: 可禁用以强制手动Phase管理
         *
         * @note 合并自Configuration::enablePhaseDetection
         */
        bool enablePhaseDetection = true;

        /**
         * @brief 启用命令性能分析
         *
         * 性能分析功能:
         * - 命令计时: 记录每个渲染命令的执行时间
         * - 统计报告: 生成性能分析报告
         * - 瓶颈定位: 识别性能热点
         *
         * 性能影响: 约5-10%开销，仅用于性能调优
         *
         * @note 合并自Configuration::enableCommandProfiling
         */
        bool enableCommandProfiling = false;

        /**
         * @brief 即时缓冲区最大大小限制
         *
         * 默认值: 16MB
         *
         * 教学要点:
         * - 内存保护: 防止即时缓冲区无限增长
         * - 性能权衡: 16MB足够大多数场景（约250K顶点）
         * - 参考Unity: Unity的动态缓冲区也有类似限制
         *
         * 使用场景:
         * - DrawVertexArray: 即时数据模式的缓冲区管理
         * - 动态扩展: 缓冲区按需扩展，但不超过此限制
         *
         * @note 参考商业引擎调查：Unity和Unreal都有缓冲区大小限制
         */
        static constexpr size_t MAX_IMMEDIATE_BUFFER_SIZE     = 1600 * 1024 * 1024; // 1600MB
        static constexpr size_t INITIAL_IMMEDIATE_BUFFER_SIZE = 640 * 1024 * 1024; // 640MB

        // ==================== 未来扩展配置 (Milestone 3.X+) ====================

        // TODO: VSync配置
        // bool enableVSync = true;

        // TODO: MSAA配置
        // int msaaSampleCount = 1; // 1=无MSAA, 2/4/8=MSAA采样数

        // TODO: HDR配置
        // bool enableHDR = false;
        // DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

        // TODO: 调试配置
        // bool enableDebugOverlay = false;
        // bool enableGPUValidation = true;

        // ==================== 静态解析方法 ====================

        /**
         * @brief 从YAML配置文件解析RendererSubsystem配置
         *
         * @param yamlPath YAML文件路径 (相对于工作目录或绝对路径)
         * @return std::optional<RendererSubsystemConfig> 解析成功返回配置，失败返回nullopt
         *
         * YAML文件结构示例 (renderer.yml):
         * @code{.yaml}
         * gbuffer:
         *   colorTexCount: 8
         * resolution:
         *   width: 1920
         *   height: 1080
         * @endcode
         *
         * 教学要点:
         * 1. 错误处理: 文件不存在时返回nullopt，记录警告
         * 2. 参数验证: 验证gbufferColorTexCount范围 [1-16]
         * 3. 默认值机制: 无效值时使用GetDefault()的默认值
         * 4. 日志记录: 记录加载成功/失败信息
         *
         * 使用示例:
         * @code{.cpp}
         * auto config = RendererSubsystemConfig::ParseFromYaml(".enigma/config/engine/renderer.yml");
         * if (config.has_value()) {
         *     // 使用配置
         *     int colorTexCount = config->gbufferColorTexCount;
         * } else {
         *     // 使用默认配置
         *     auto defaultConfig = RendererSubsystemConfig::GetDefault();
         * }
         * @endcode
         *
         * @note 架构参考: ScheduleSubsystem.cpp:45-54 (ScheduleConfig::LoadFromFile)
         */
        static std::optional<RendererSubsystemConfig> ParseFromYaml(const std::string& yamlPath);

        /**
         * @brief 获取默认配置（安全回退机制）
         *
         * @return RendererSubsystemConfig 默认配置
         *
         * 默认配置值:
         * - gbufferColorTexCount: 8 (平衡内存和功能)
         * - renderWidth: 1920 (Full HD)
         * - renderHeight: 1080 (Full HD)
         *
         * 教学要点:
         * 1. 安全回退: 配置文件缺失时的保证
         * 2. 合理默认值: 适用于大多数开发场景
         * 3. 静态方法: 无需实例化即可获取默认配置
         *
         * 使用示例:
         * @code{.cpp}
         * // 配置文件加载失败时的回退
         * auto config = RendererSubsystemConfig::ParseFromYaml("renderer.yml");
         * if (!config.has_value()) {
         *     config = RendererSubsystemConfig::GetDefault();
         *     LogWarn("Using default RendererSubsystem config");
         * }
         * @endcode
         *
         * @note 架构参考: ScheduleSubsystem.cpp:59-69 (ScheduleConfig::GetDefaultConfig)
         */
        static RendererSubsystemConfig GetDefault();
    };
} // namespace enigma::graphic
