#include "ShaderPack.hpp"
#include "Include/AbsolutePackPath.hpp"
#include "Include/ShaderPackSourceNames.hpp"
#include "Engine/Graphic/Core/Pipeline/PipelineManager.hpp"  // Phase 4.4 - NamespacedId 支持
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // detail 命名空间 - 辅助扫描函数（私有实现）
    // ========================================================================

    namespace detail
    {
        /**
         * @brief 扫描 shaders/ 目录下的所有着色器文件
         * @param shadersDir shaders/ 目录路径（文件系统路径）
         * @return 所有着色器程序文件的 Shader Pack 内部路径列表
         *
         * **教学要点**:
         * - 扫描所有 .vs.hlsl 和 .ps.hlsl 文件
         * - 转换为 Shader Pack 内部路径（/shaders/文件名）
         * - 这些路径作为 IncludeGraph 的起始节点
         *
         * **业务逻辑**:
         * 1. 遍历 shaders/ 目录
         * 2. 查找所有 .vs.hlsl 和 .ps.hlsl 文件
         * 3. 构造 AbsolutePackPath（Unix 风格路径）
         */
        std::vector<AbsolutePackPath> ScanStartingPaths(const std::filesystem::path& shadersDir)
        {
            std::vector<AbsolutePackPath> startingPaths;

            if (!std::filesystem::exists(shadersDir))
            {
                return startingPaths;
            }

            for (const auto& entry : std::filesystem::directory_iterator(shadersDir))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const auto& path     = entry.path();
                const auto  filename = path.filename().string();

                // 检测是否为顶点着色器 (.vs.hlsl)
                bool isVertexShader = (filename.size() >= 8 && filename.substr(filename.size() - 8) == ".vs.hlsl");
                // 检测是否为像素着色器 (.ps.hlsl) - DirectX 术语
                bool isPixelShader = (filename.size() >= 8 && filename.substr(filename.size() - 8) == ".ps.hlsl");

                if (isVertexShader || isPixelShader)
                {
                    // 构造 Shader Pack 内部路径：/shaders/文件名
                    // 例如：F:/shaderpacks/MyPack/shaders/gbuffers_terrain.vs.hlsl
                    //       → /shaders/gbuffers_terrain.vs.hlsl
                    std::string packPath = "/shaders/" + filename;
                    startingPaths.push_back(AbsolutePackPath::FromAbsolutePath(packPath));
                }
            }

            return startingPaths;
        }
    } // namespace detail

    // ========================================================================
    // ShaderPack 构造函数实现（协调器模式）⭐
    // ========================================================================

    ShaderPack::ShaderPack(const std::filesystem::path& root)
        : m_root(std::filesystem::absolute(root)) // + 转换为绝对路径
    {
        /**
         * 教学要点：协调器模式 - 按依赖顺序初始化所有子系统
         *
         * 初始化顺序至关重要：
         * 1. IncludeGraph 不依赖任何子系统（先初始化）+ Phase 1
         * 2. ShaderProperties 不依赖 IncludeGraph（可并行）+ Phase 0.5
         * 3. ShaderPackOptions 依赖 IncludeGraph 和 ShaderProperties（后初始化）⚠️ Phase 2
         * 4. ProgramSet 依赖 IncludeGraph 和 ShaderProperties（最后初始化）⚠️ Phase 3-4
         *
         * **路径处理修复 (2025-10-14)**:
         * - 使用 std::filesystem::absolute() 将相对路径转换为绝对路径
         * - 使用 std::filesystem::canonical() 规范化路径（统一分隔符为 /）
         * - 确保所有文件系统操作基于正确的绝对路径
         * - 避免相对路径在不同工作目录下解析失败
         */

        // ========================================================================
        // Step 0: 验证路径存在性并规范化路径
        // ========================================================================
        if (!std::filesystem::exists(m_root))
        {
            DebuggerPrintf("[ShaderPack] ERROR: Root path does not exist: '%s'\n", m_root.string().c_str());
            ERROR_AND_DIE("ShaderPack root path does not exist");
        }

        // + 规范化路径（解析 . 和 ..，统一分隔符为 /）
        m_root = std::filesystem::canonical(m_root);

        DebuggerPrintf("[ShaderPack] Using canonical absolute path: '%s'\n", m_root.string().c_str());

        // ========================================================================
        // Step 1: 扫描起始路径（shaders/ 目录下的所有着色器文件）
        // ========================================================================
        auto shadersDir    = m_root / "shaders";
        auto startingPaths = detail::ScanStartingPaths(shadersDir);

        // ========================================================================
        // Step 2: 构建 IncludeGraph（BFS 加载所有依赖）+ Phase 1
        // ========================================================================
        m_includeGraph = std::make_unique<IncludeGraph>(m_root, startingPaths);

        // ========================================================================
        // Step 3: 解析 ShaderProperties（读取 shaders.properties）+ Phase 0.5
        // ========================================================================
        m_shaderProperties = std::make_unique<ShaderProperties>();

        // 调用 Parse() 方法加载 shaders.properties
        // 路径: <root>/shaders/shaders.properties
        bool propertiesParsed = m_shaderProperties->Parse(m_root);

        if (!propertiesParsed)
        {
            // 解析失败，记录警告但不中断初始化（对齐 Iris 行为）
            // Iris 也是允许没有 shaders.properties 的 Shader Pack 运行
            DebuggerPrintf("[ShaderPack] Warning: Failed to parse shaders.properties at '%s'\n",
                           (m_root / "shaders" / "shaders.properties").string().c_str());
        }

        // ========================================================================
        // Step 4: 创建 ShaderPackOptions（基于 shaderpack.properties）+ Phase 2
        // ========================================================================
        m_options = std::make_unique<ShaderPackOptions>();

        // 调用 Parse() 方法加载 shaderpack.properties
        bool optionsParsed = m_options->Parse(m_root);

        if (!optionsParsed)
        {
            // 解析失败，记录警告但不中断初始化（对齐 Iris 宽松策略）
            // shaderpack.properties 是可选文件
            DebuggerPrintf("[ShaderPack] Warning: Failed to parse shaderpack.properties at '%s'\n",
                           (m_root / "shaderpack.properties").string().c_str());
        }
        else
        {
            // 成功加载选项，输出宏定义数量
            auto macros = m_options->GetMacroDefinitions();
            DebuggerPrintf("[ShaderPack] Loaded %zu macro definitions from options\n", macros.size());
        }

        // ========================================================================
        // Step 5: 加载默认维度（world0）的 ProgramSet
        // + Phase 4.4 - 使用延迟加载机制，首次调用 GetProgramSet("world0") 时自动加载
        // ========================================================================
        // 教学要点：
        // - 构造函数不再直接加载 ProgramSet，而是触发延迟加载
        // - 所有加载逻辑移动到 LoadDimensionProgramSet() 中
        // - 这样可以复用相同的加载逻辑处理所有维度（world0, world1, world-1 等）

        // 触发默认维度（world0）的加载
        ProgramSet* defaultProgramSet = GetProgramSet("world0");

        if (!defaultProgramSet)
        {
            DebuggerPrintf("[ShaderPack] Warning: Failed to load default ProgramSet (world0)\n");
        }
        else
        {
            // 输出加载统计
            DebuggerPrintf("[ShaderPack] Default ProgramSet (world0) loaded: %zu single programs, %zu array programs\n",
                           defaultProgramSet->GetLoadedProgramCount(),
                           defaultProgramSet->GetLoadedArrayCount());
        }
    }

    // ========================================================================
    // IsValid() 实现
    // ========================================================================

    bool ShaderPack::IsValid() const
    {
        /**
         * 教学要点：分阶段验证策略
         *
         * Phase 1: 只验证 IncludeGraph
         * Phase 2-3: 验证 ShaderProperties
         * Phase 4: 验证 ProgramSet
         * Phase 5: 验证 ShaderPackOptions
         */

        // ========================================================================
        // Phase 1 验证：IncludeGraph 必须成功加载
        // ========================================================================
        if (!m_includeGraph)
        {
            return false;
        }

        // 检查 IncludeGraph 加载失败率
        const auto& failures = m_includeGraph->GetFailures();
        const auto& nodes    = m_includeGraph->GetNodes();

        // 如果所有文件都加载失败，则认为 Shader Pack 无效
        if (failures.empty() && nodes.empty())
        {
            return false; // 没有加载任何文件
        }

        // 如果失败率超过50%，认为 Shader Pack 有严重问题
        size_t totalFiles = failures.size() + nodes.size();
        if (failures.size() > totalFiles * 0.5)
        {
            return false; // 超过50%文件加载失败
        }

        // ========================================================================
        // Phase 0.5 验证：ShaderProperties 解析成功 +
        // ========================================================================
        // 注意：ShaderProperties 解析失败不影响 Shader Pack 有效性
        // Iris 允许没有 shaders.properties 的 Shader Pack 运行（使用默认配置）
        // 只验证对象存在即可
        if (!m_shaderProperties)
        {
            return false;
        }

        // 可选：检查 IsValid() 状态（如果想要强制要求配置文件）
        // if (!m_shaderProperties->IsValid())
        // {
        //     return false;
        // }

        // ========================================================================
        // Phase 2 验证：ShaderPackOptions 存在 +
        // ========================================================================
        // 注意：ShaderPackOptions 解析失败不影响 Shader Pack 有效性
        // shaderpack.properties 是可选文件，只验证对象存在即可
        if (!m_options)
        {
            return false;
        }

        // ========================================================================
        // Phase 4.4 验证：默认维度（world0）的 ProgramSet 至少加载了 Basic 程序
        // + Phase 4.4 实现 - 支持多维度验证
        // ========================================================================
        // 教学要点：
        // - 使用 const_cast 绕过 const 限制调用 GetProgramSet()
        // - 验证默认维度（world0）的 ProgramSet 是否有效
        // - 其他维度的 ProgramSet 是延迟加载的，不在初始化时验证

        const ProgramSet* defaultProgramSet = this->GetProgramSet(); // 调用 const 版本，内部调用 GetProgramSet("world0")

        if (!defaultProgramSet)
        {
            DebuggerPrintf("[ShaderPack] Error: Default ProgramSet (world0) not loaded\n");
            return false;
        }

        // 使用 ProgramSet::Validate() 方法验证 world0
        if (!defaultProgramSet->Validate())
        {
            DebuggerPrintf("[ShaderPack] Warning: Default ProgramSet (world0) validation failed\n");
            return false;
        }

        return true;
    }

    // ========================================================================
    // Phase 4.4 - 混合维度系统实现
    // ========================================================================

    /**
     * @brief 获取指定维度的 ProgramSet（字符串模式 - 主接口）
     *
     * **教学要点**:
     * - Lazy Loading Pattern - 首次访问时才创建
     * - 使用 unordered_map 实现维度缓存
     * - 自动触发 LoadDimensionProgramSet() 加载
     * - 返回原始指针以保持接口简洁
     *
     * **Iris 对应**:
     * - net.irisshaders.iris.pipeline.PipelineManager.preparePipeline(NamespacedId)
     * - 在 Iris 中,PipelineManager 管理多个维度的 WorldRenderingPipeline
     * - 我们这里管理多个维度的 ProgramSet
     */
    ProgramSet* ShaderPack::GetProgramSet(const std::string& dimensionName)
    {
        // ========================================================================
        // Step 1: 查找缓存中是否已存在该维度的 ProgramSet
        // ========================================================================
        auto it = m_programSets.find(dimensionName);
        if (it != m_programSets.end())
        {
            // 缓存命中,直接返回
            return it->second.get();
        }

        // ========================================================================
        // Step 2: 缓存未命中,延迟加载该维度的 ProgramSet
        // ========================================================================
        DebuggerPrintf("[ShaderPack] Loading ProgramSet for dimension '%s' (Lazy Loading)\n", dimensionName.c_str());

        auto programSet = LoadDimensionProgramSet(dimensionName);

        if (!programSet)
        {
            // 加载失败,返回 nullptr
            DebuggerPrintf("[ShaderPack] Error: Failed to load ProgramSet for dimension '%s'\n", dimensionName.c_str());
            return nullptr;
        }

        // ========================================================================
        // Step 3: 将加载的 ProgramSet 添加到缓存中
        // ========================================================================
        ProgramSet* rawPtr           = programSet.get();
        m_programSets[dimensionName] = std::move(programSet);

        DebuggerPrintf("[ShaderPack] Successfully loaded ProgramSet for dimension '%s'\n", dimensionName.c_str());

        return rawPtr;
    }

    /**
     * @brief 获取指定维度的 ProgramSet（NamespacedId 模式 - 兼容接口）
     *
     * **教学要点**:
     * - Adapter Pattern - 将 NamespacedId 转换为字符串
     * - 内部调用主接口 GetProgramSet(string)
     * - 保持与 PipelineManager 的互操作性
     *
     * **Iris 对应**:
     * - net.irisshaders.iris.pipeline.PipelineManager.preparePipeline(NamespacedId)
     * - NamespacedId 是 Minecraft 标准的维度标识符格式
     */
    ProgramSet* ShaderPack::GetProgramSet(const NamespacedId& dimension)
    {
        // 使用 Adapter Pattern 转换 NamespacedId → 字符串
        std::string dimensionName = NamespacedIdToDirectoryName(dimension);

        DebuggerPrintf("[ShaderPack] GetProgramSet: NamespacedId '%s' → Directory '%s'\n",
                       dimension.ToString().c_str(),
                       dimensionName.c_str());

        // 调用主接口
        return GetProgramSet(dimensionName);
    }

    /**
     * @brief 检测 Base ProgramSet 的默认程序目录（Iris 标准优先级）
     *
     * **教学要点**:
     * - Iris 标准优先级: world0/ > program/ > shaders/
     * - program/ 是 Iris 标准的默认程序目录（单数，不是 programs/）
     * - world0/ 如果存在，优先级最高（作为默认维度）
     * - 这个检测结果影响所有维度的 Fallback Chain 行为
     *
     * **Iris 对应**:
     * - net.irisshaders.iris.pipeline.ProgramSet.scanDirectoryForPrograms()
     * - Iris 在扫描时会检测 world0/ 和 program/ 目录
     *
     * **返回值**:
     * - "world0"  - 如果 shaders/world0/ 存在
     * - "program" - 如果 shaders/program/ 存在
     * - ""        - 回退到 shaders/ 根目录
     */
    std::string ShaderPack::DetectBaseProgramDirectory() const
    {
        // ========================================================================
        // 步骤 1: 检测 world0 目录（最高优先级）⭐⭐⭐
        // ========================================================================
        std::filesystem::path world0Dir = m_root / "shaders" / "world0";
        if (std::filesystem::exists(world0Dir))
        {
            DebuggerPrintf("[ShaderPack] Detected world0/ directory, using as base program directory\n");
            return "world0";
        }

        // ========================================================================
        // 步骤 2: 检测 program 目录（次高优先级）⭐
        // ========================================================================
        std::filesystem::path programDir = m_root / "shaders" / "program";
        if (std::filesystem::exists(programDir))
        {
            DebuggerPrintf("[ShaderPack] Detected program/ directory, using as base program directory\n");
            return "program";
        }

        // ========================================================================
        // 步骤 3: 回退到 shaders/ 根目录（最低优先级）
        // ========================================================================
        DebuggerPrintf("[ShaderPack] No world0/ or program/ directory found, using shaders/ root as base\n");
        return "";
    }

    /**
     * @brief NamespacedId 到目录名的转换（Adapter Pattern）
     *
     * **教学要点**:
     * - Iris 标准维度目录约定:
     *   - minecraft:overworld  → "world0"
     *   - minecraft:the_nether → "world-1"
     *   - minecraft:the_end    → "world1"
     * - 其他命名空间直接使用 path（支持自定义维度）
     *
     * **Iris 对应**:
     * - net.irisshaders.iris.pipeline.DimensionId.getId()
     * - Iris 使用固定的目录名约定来定位维度覆盖着色器
     */
    std::string ShaderPack::NamespacedIdToDirectoryName(const NamespacedId& id)
    {
        // ========================================================================
        // Iris 标准维度映射
        // ========================================================================
        if (id.nameSpace == "minecraft")
        {
            if (id.path == "overworld")
            {
                return "world0"; // 主世界（Overworld）
            }
            else if (id.path == "the_nether")
            {
                return "world-1"; // 下界（Nether）
            }
            else if (id.path == "the_end")
            {
                return "world1"; // 末地（End）
            }
        }

        // ========================================================================
        // 其他命名空间 或 自定义维度
        // ========================================================================
        // 直接使用 path 作为目录名（如 "world2", "world3", "world99"）
        // 这支持自定义维度（如 Mod 添加的维度）
        return id.path;
    }

    /**
     * @brief 加载特定维度的 ProgramSet（延迟加载核心逻辑）
     *
     * **教学要点**:
     * - Lazy Loading Pattern - 首次访问时才创建
     * - 自动扫描维度覆盖目录（shaders/worldX/）
     * - Fallback 到 world0（如果维度目录不存在）
     * - 复用 IncludeGraph 节点加载逻辑
     *
     * **实现流程**:
     * 1. 构造维度覆盖目录路径（如 "shaders/world1/"）
     * 2. 检查该维度目录是否存在（通过文件系统）
     * 3. 如果存在，优先搜索维度目录
     * 4. 如果不存在，Fallback 到默认目录（"shaders/"）
     * 5. 加载所有47个单一程序和6个程序数组
     *
     * **Iris 对应**:
     * - net.irisshaders.iris.pipeline.ProgramSet.scanDirectoryForPrograms()
     * - Iris 也使用目录扫描机制查找维度覆盖着色器
     */
    std::unique_ptr<ProgramSet> ShaderPack::LoadDimensionProgramSet(const std::string& dimensionName)
    {
        DebuggerPrintf("[ShaderPack] LoadDimensionProgramSet: Starting load for dimension '%s'\n", dimensionName.c_str());

        // ========================================================================
        // Step 1: 构造维度覆盖目录路径
        // ========================================================================
        // 例如: "shaders/world1/" 用于末地维度
        std::filesystem::path dimensionDir    = m_root / "shaders" / dimensionName;
        std::string           dimensionPrefix = "/shaders/" + dimensionName + "/";

        // ========================================================================
        // Step 2: 检查维度目录是否存在（优先级最高）⭐⭐⭐
        // ========================================================================
        bool hasDimensionOverride = std::filesystem::exists(dimensionDir);

        if (hasDimensionOverride)
        {
            DebuggerPrintf("[ShaderPack] Dimension override directory found: '%s'\n", dimensionDir.string().c_str());
        }
        else
        {
            DebuggerPrintf("[ShaderPack] No dimension override directory for '%s'\n", dimensionName.c_str());
        }

        // ========================================================================
        // Step 2.5: 检测 program/ 目录是否存在（优先级中等）⭐
        // ========================================================================
        std::filesystem::path programDir    = m_root / "shaders" / "program";
        bool                  hasProgramDir = std::filesystem::exists(programDir);

        if (hasProgramDir)
        {
            DebuggerPrintf("[ShaderPack] program/ directory found, will be used as fallback\n");
        }

        // ========================================================================
        // Step 3: 创建新的 ProgramSet
        // ========================================================================
        auto programSet = std::make_unique<ProgramSet>();

        // ========================================================================
        // Step 4: 加载所有单一程序（47 个 ProgramId）⭐⭐⭐
        // 实现完整的三级 Fallback Chain（Iris 标准）
        // ========================================================================
        for (int i = 0; i < static_cast<int>(ProgramId::COUNT); ++i)
        {
            ProgramId   id         = static_cast<ProgramId>(i);
            std::string sourceName = ProgramIdToSourceName(id);

            bool foundProgram = false;

            // ====================================================================
            // 优先级 1: 维度覆盖目录（shaders/worldX/）⭐⭐⭐
            // ====================================================================
            if (hasDimensionOverride)
            {
                // 构造维度覆盖路径: /shaders/world1/gbuffers_terrain.vs.hlsl
                AbsolutePackPath vsPath = AbsolutePackPath::FromAbsolutePath(dimensionPrefix + sourceName + ".vs.hlsl");
                AbsolutePackPath psPath = AbsolutePackPath::FromAbsolutePath(dimensionPrefix + sourceName + ".ps.hlsl");

                // 检查 IncludeGraph 中是否存在
                auto vsNodeOpt = m_includeGraph->GetNode(vsPath);
                auto psNodeOpt = m_includeGraph->GetNode(psPath);

                if (vsNodeOpt.has_value() && psNodeOpt.has_value())
                {
                    // 获取源码
                    const auto& vsLines = vsNodeOpt.value().GetLines();
                    const auto& psLines = psNodeOpt.value().GetLines();

                    // ========================================================================
                    // 三层验证（Phase 4.4 - 空文件过滤）
                    // ========================================================================

                    // + 第一层：检查文件行数非空
                    if (vsLines.empty() || psLines.empty())
                    {
                        DebuggerPrintf("[ShaderPack] Warning: '%s' has 0 lines (empty file) in dimension directory, trying next fallback\n",
                                       sourceName.c_str());
                        // 不设置foundProgram，让代码继续尝试下一级Fallback
                    }
                    else
                    {
                        // 拼接源码
                        std::string vsSource, psSource;
                        for (const auto& line : vsLines)
                        {
                            vsSource += line + "\n";
                        }
                        for (const auto& line : psLines)
                        {
                            psSource += line + "\n";
                        }

                        // 创建 ShaderSource
                        auto shaderSource = std::make_unique<ShaderSource>(sourceName, vsSource, psSource);

                        // + 第二层：IsValid()检查（Iris兼容性）
                        if (!shaderSource->IsValid())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: '%s' failed IsValid() check in dimension directory, trying next fallback\n",
                                           sourceName.c_str());
                        }
                        // + 第三层：HasNonEmptySource()检查（严格内容验证）
                        else if (!shaderSource->HasNonEmptySource())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: '%s' has no non-whitespace content in dimension directory, trying next fallback\n",
                                           sourceName.c_str());
                        }
                        else
                        {
                            // + 所有验证通过，注册着色器
                            foundProgram = true;
                            programSet->RegisterProgram(id, std::move(shaderSource));
                            DebuggerPrintf("[ShaderPack] + Registered '%s' from dimension directory '%s' (VS=%zu lines, PS=%zu lines)\n",
                                           sourceName.c_str(),
                                           dimensionName.c_str(),
                                           shaderSource->GetVertexLineCount(),
                                           shaderSource->GetPixelLineCount());
                        }
                    }
                }
            }

            // ====================================================================
            // 优先级 2: program/ 目录回退（shaders/program/）⭐⭐
            // ====================================================================
            if (!foundProgram && hasProgramDir)
            {
                // 构造 program/ 路径: /shaders/program/gbuffers_terrain.vs.hlsl
                AbsolutePackPath vsPath = AbsolutePackPath::FromAbsolutePath("/shaders/program/" + sourceName + ".vs.hlsl");
                AbsolutePackPath psPath = AbsolutePackPath::FromAbsolutePath("/shaders/program/" + sourceName + ".ps.hlsl");

                auto vsNodeOpt = m_includeGraph->GetNode(vsPath);
                auto psNodeOpt = m_includeGraph->GetNode(psPath);

                if (vsNodeOpt.has_value() && psNodeOpt.has_value())
                {
                    // 获取源码
                    const auto& vsLines = vsNodeOpt.value().GetLines();
                    const auto& psLines = psNodeOpt.value().GetLines();

                    // ========================================================================
                    // 三层验证（Phase 4.4 - 空文件过滤）
                    // ========================================================================

                    // + 第一层：检查文件行数非空
                    if (vsLines.empty() || psLines.empty())
                    {
                        DebuggerPrintf("[ShaderPack] Warning: '%s' has 0 lines (empty file) in program/ directory, trying next fallback\n",
                                       sourceName.c_str());
                        // 不设置foundProgram，让代码继续尝试下一级Fallback
                    }
                    else
                    {
                        // 拼接源码
                        std::string vsSource, psSource;
                        for (const auto& line : vsLines)
                        {
                            vsSource += line + "\n";
                        }
                        for (const auto& line : psLines)
                        {
                            psSource += line + "\n";
                        }

                        // 创建 ShaderSource
                        auto shaderSource = std::make_unique<ShaderSource>(sourceName, vsSource, psSource);

                        // + 第二层：IsValid()检查（Iris兼容性）
                        if (!shaderSource->IsValid())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: '%s' failed IsValid() check in program/ directory, trying next fallback\n",
                                           sourceName.c_str());
                        }
                        // + 第三层：HasNonEmptySource()检查（严格内容验证）
                        else if (!shaderSource->HasNonEmptySource())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: '%s' has no non-whitespace content in program/ directory, trying next fallback\n",
                                           sourceName.c_str());
                        }
                        else
                        {
                            // + 所有验证通过，注册着色器
                            foundProgram = true;
                            programSet->RegisterProgram(id, std::move(shaderSource));
                            DebuggerPrintf("[ShaderPack] + Registered '%s' from program/ directory (VS=%zu lines, PS=%zu lines)\n",
                                           sourceName.c_str(),
                                           shaderSource->GetVertexLineCount(),
                                           shaderSource->GetPixelLineCount());
                        }
                    }
                }
            }

            // ====================================================================
            // 优先级 3: shaders/ 根目录回退 ⭐
            // ====================================================================
            if (!foundProgram)
            {
                // 构造默认路径: /shaders/gbuffers_terrain.vs.hlsl
                AbsolutePackPath vsPath = AbsolutePackPath::FromAbsolutePath("/shaders/" + sourceName + ".vs.hlsl");
                AbsolutePackPath psPath = AbsolutePackPath::FromAbsolutePath("/shaders/" + sourceName + ".ps.hlsl");

                auto vsNodeOpt = m_includeGraph->GetNode(vsPath);
                auto psNodeOpt = m_includeGraph->GetNode(psPath);

                if (vsNodeOpt.has_value() && psNodeOpt.has_value())
                {
                    // 获取源码
                    const auto& vsLines = vsNodeOpt.value().GetLines();
                    const auto& psLines = psNodeOpt.value().GetLines();

                    // ========================================================================
                    // 三层验证（Phase 4.4 - 空文件过滤）
                    // ========================================================================

                    // + 第一层：检查文件行数非空
                    if (vsLines.empty() || psLines.empty())
                    {
                        DebuggerPrintf("[ShaderPack] Warning: '%s' has 0 lines (empty file) in shaders/ root, skipping\n",
                                       sourceName.c_str());
                        // 这是最后一级Fallback，直接跳过，不再尝试
                    }
                    else
                    {
                        // 拼接源码
                        std::string vsSource, psSource;
                        for (const auto& line : vsLines)
                        {
                            vsSource += line + "\n";
                        }
                        for (const auto& line : psLines)
                        {
                            psSource += line + "\n";
                        }

                        // 创建 ShaderSource
                        auto shaderSource = std::make_unique<ShaderSource>(sourceName, vsSource, psSource);

                        // + 第二层：IsValid()检查（Iris兼容性）
                        if (!shaderSource->IsValid())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: '%s' failed IsValid() check in shaders/ root, skipping\n",
                                           sourceName.c_str());
                        }
                        // + 第三层：HasNonEmptySource()检查（严格内容验证）
                        else if (!shaderSource->HasNonEmptySource())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: '%s' has no non-whitespace content in shaders/ root, skipping\n",
                                           sourceName.c_str());
                        }
                        else
                        {
                            // + 所有验证通过，注册着色器
                            foundProgram = true;
                            programSet->RegisterProgram(id, std::move(shaderSource));
                            DebuggerPrintf("[ShaderPack] + Registered '%s' from shaders/ root (VS=%zu lines, PS=%zu lines)\n",
                                           sourceName.c_str(),
                                           shaderSource->GetVertexLineCount(),
                                           shaderSource->GetPixelLineCount());
                        }
                    }
                }
            }

            // ====================================================================
            // 所有 Fallback 都失败
            // ====================================================================
            if (!foundProgram)
            {
                // 可选：添加调试输出
                // DebuggerPrintf("[ShaderPack] Program '%s' not found in any directory\n", sourceName.c_str());
            }
        }

        // ========================================================================
        // Step 5: 加载程序数组（6 个 ProgramArrayId）⭐⭐⭐
        // 实现完整的三级 Fallback Chain（Iris 标准）
        // ========================================================================
        for (int arrayIdx = 0; arrayIdx < static_cast<int>(ProgramArrayId::COUNT); ++arrayIdx)
        {
            ProgramArrayId arrayId  = static_cast<ProgramArrayId>(arrayIdx);
            std::string    baseName = GetProgramArrayPrefix(arrayId);
            int            maxSlots = GetProgramArraySlotCount(arrayId);

            for (int slotIndex = 0; slotIndex < maxSlots; ++slotIndex)
            {
                std::string programName = GetProgramArraySlotName(arrayId, slotIndex);

                bool foundProgram = false;

                // ================================================================
                // 优先级 1: 维度覆盖目录（shaders/worldX/）⭐⭐⭐
                // ================================================================
                if (hasDimensionOverride)
                {
                    AbsolutePackPath vsPath = AbsolutePackPath::FromAbsolutePath(dimensionPrefix + programName + ".vs.hlsl");
                    AbsolutePackPath psPath = AbsolutePackPath::FromAbsolutePath(dimensionPrefix + programName + ".ps.hlsl");

                    auto vsNodeOpt = m_includeGraph->GetNode(vsPath);
                    auto psNodeOpt = m_includeGraph->GetNode(psPath);

                    if (vsNodeOpt.has_value() && psNodeOpt.has_value())
                    {
                        const auto& vsLines = vsNodeOpt.value().GetLines();
                        const auto& psLines = psNodeOpt.value().GetLines();

                        // ========================================================================
                        // 三层验证（Phase 4.4 - 空文件过滤）
                        // ========================================================================

                        // + 第一层：检查文件行数非空
                        if (vsLines.empty() || psLines.empty())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] has 0 lines (empty file) in dimension directory, trying next fallback\n",
                                           programName.c_str(), slotIndex);
                            // 不设置foundProgram，让代码继续尝试下一级Fallback
                        }
                        else
                        {
                            // 拼接源码
                            std::string vsSource, psSource;
                            for (const auto& line : vsLines)
                            {
                                vsSource += line + "\n";
                            }
                            for (const auto& line : psLines)
                            {
                                psSource += line + "\n";
                            }

                            // 创建 ShaderSource
                            auto shaderSource = std::make_unique<ShaderSource>(programName, vsSource, psSource);

                            // + 第二层：IsValid()检查（Iris兼容性）
                            if (!shaderSource->IsValid())
                            {
                                DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] failed IsValid() check in dimension directory, trying next fallback\n",
                                               programName.c_str(), slotIndex);
                            }
                            // + 第三层：HasNonEmptySource()检查（严格内容验证）
                            else if (!shaderSource->HasNonEmptySource())
                            {
                                DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] has no non-whitespace content in dimension directory, trying next fallback\n",
                                               programName.c_str(), slotIndex);
                            }
                            else
                            {
                                // + 所有验证通过，注册着色器
                                foundProgram = true;
                                programSet->RegisterArrayProgram(arrayId, static_cast<size_t>(slotIndex), std::move(shaderSource));
                                DebuggerPrintf("[ShaderPack] + Registered array '%s'[%d] from dimension directory '%s' (VS=%zu lines, PS=%zu lines)\n",
                                               programName.c_str(),
                                               slotIndex,
                                               dimensionName.c_str(),
                                               shaderSource->GetVertexLineCount(),
                                               shaderSource->GetPixelLineCount());
                            }
                        }
                    }
                }

                // ================================================================
                // 优先级 2: program/ 目录回退（shaders/program/）⭐⭐
                // ================================================================
                if (!foundProgram && hasProgramDir)
                {
                    AbsolutePackPath vsPath = AbsolutePackPath::FromAbsolutePath("/shaders/program/" + programName + ".vs.hlsl");
                    AbsolutePackPath psPath = AbsolutePackPath::FromAbsolutePath("/shaders/program/" + programName + ".ps.hlsl");

                    auto vsNodeOpt = m_includeGraph->GetNode(vsPath);
                    auto psNodeOpt = m_includeGraph->GetNode(psPath);

                    if (vsNodeOpt.has_value() && psNodeOpt.has_value())
                    {
                        const auto& vsLines = vsNodeOpt.value().GetLines();
                        const auto& psLines = psNodeOpt.value().GetLines();

                        // ========================================================================
                        // 三层验证（Phase 4.4 - 空文件过滤）
                        // ========================================================================

                        // + 第一层：检查文件行数非空
                        if (vsLines.empty() || psLines.empty())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] has 0 lines (empty file) in program/ directory, trying next fallback\n",
                                           programName.c_str(), slotIndex);
                            // 不设置foundProgram，让代码继续尝试下一级Fallback
                        }
                        else
                        {
                            // 拼接源码
                            std::string vsSource, psSource;
                            for (const auto& line : vsLines)
                            {
                                vsSource += line + "\n";
                            }
                            for (const auto& line : psLines)
                            {
                                psSource += line + "\n";
                            }

                            // 创建 ShaderSource
                            auto shaderSource = std::make_unique<ShaderSource>(programName, vsSource, psSource);

                            // + 第二层：IsValid()检查（Iris兼容性）
                            if (!shaderSource->IsValid())
                            {
                                DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] failed IsValid() check in program/ directory, trying next fallback\n",
                                               programName.c_str(), slotIndex);
                            }
                            // + 第三层：HasNonEmptySource()检查（严格内容验证）
                            else if (!shaderSource->HasNonEmptySource())
                            {
                                DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] has no non-whitespace content in program/ directory, trying next fallback\n",
                                               programName.c_str(), slotIndex);
                            }
                            else
                            {
                                // + 所有验证通过，注册着色器
                                foundProgram = true;
                                programSet->RegisterArrayProgram(arrayId, static_cast<size_t>(slotIndex), std::move(shaderSource));
                                DebuggerPrintf("[ShaderPack] + Registered array '%s'[%d] from program/ directory (VS=%zu lines, PS=%zu lines)\n",
                                               programName.c_str(),
                                               slotIndex,
                                               shaderSource->GetVertexLineCount(),
                                               shaderSource->GetPixelLineCount());
                            }
                        }
                    }
                }

                // ================================================================
                // 优先级 3: shaders/ 根目录回退 ⭐
                // ================================================================
                if (!foundProgram)
                {
                    AbsolutePackPath vsPath = AbsolutePackPath::FromAbsolutePath("/shaders/" + programName + ".vs.hlsl");
                    AbsolutePackPath psPath = AbsolutePackPath::FromAbsolutePath("/shaders/" + programName + ".ps.hlsl");

                    auto vsNodeOpt = m_includeGraph->GetNode(vsPath);
                    auto psNodeOpt = m_includeGraph->GetNode(psPath);

                    if (vsNodeOpt.has_value() && psNodeOpt.has_value())
                    {
                        const auto& vsLines = vsNodeOpt.value().GetLines();
                        const auto& psLines = psNodeOpt.value().GetLines();

                        // ========================================================================
                        // 三层验证（Phase 4.4 - 空文件过滤）
                        // ========================================================================

                        // + 第一层：检查文件行数非空
                        if (vsLines.empty() || psLines.empty())
                        {
                            DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] has 0 lines (empty file) in shaders/ root, skipping\n",
                                           programName.c_str(), slotIndex);
                            // 这是最后一级Fallback，直接跳过，不再尝试
                        }
                        else
                        {
                            // 拼接源码
                            std::string vsSource, psSource;
                            for (const auto& line : vsLines)
                            {
                                vsSource += line + "\n";
                            }
                            for (const auto& line : psLines)
                            {
                                psSource += line + "\n";
                            }

                            // 创建 ShaderSource
                            auto shaderSource = std::make_unique<ShaderSource>(programName, vsSource, psSource);

                            // + 第二层：IsValid()检查（Iris兼容性）
                            if (!shaderSource->IsValid())
                            {
                                DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] failed IsValid() check in shaders/ root, skipping\n",
                                               programName.c_str(), slotIndex);
                            }
                            // + 第三层：HasNonEmptySource()检查（严格内容验证）
                            else if (!shaderSource->HasNonEmptySource())
                            {
                                DebuggerPrintf("[ShaderPack] Warning: Array '%s'[%d] has no non-whitespace content in shaders/ root, skipping\n",
                                               programName.c_str(), slotIndex);
                            }
                            else
                            {
                                // + 所有验证通过，注册着色器
                                foundProgram = true;
                                programSet->RegisterArrayProgram(arrayId, static_cast<size_t>(slotIndex), std::move(shaderSource));
                                DebuggerPrintf("[ShaderPack] + Registered array '%s'[%d] from shaders/ root (VS=%zu lines, PS=%zu lines)\n",
                                               programName.c_str(),
                                               slotIndex,
                                               shaderSource->GetVertexLineCount(),
                                               shaderSource->GetPixelLineCount());
                            }
                        }
                    }
                }

                // 程序数组允许稀疏，不输出警告
            }
        }

        // ========================================================================
        // Step 6: 输出加载统计
        // ========================================================================
        DebuggerPrintf("[ShaderPack] LoadDimensionProgramSet: Loaded %zu single programs, %zu array programs for dimension '%s'\n",
                       programSet->GetLoadedProgramCount(),
                       programSet->GetLoadedArrayCount(),
                       dimensionName.c_str());

        return programSet;
    }
} // namespace enigma::graphic
