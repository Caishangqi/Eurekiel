#pragma once

#include "ProgramId.hpp"       // Iris 单一程序ID枚举
#include "ProgramArrayId.hpp"  // Iris 程序数组ID枚举
#include "Include/IncludeGraph.hpp"  // Include 依赖图管理
#include "Properties/ShaderProperties.hpp"  // Shader Pack 配置解析
#include "Option/ShaderPackOptions.hpp"  // Shader Pack 选项管理
#include "ProgramSet.hpp"  // 着色器程序集管理
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <unordered_map>
#include <memory>
#include <filesystem>

namespace enigma::graphic
{
    // 前向声明（Phase 4.4 - NamespacedId 支持）
    struct NamespacedId;

    /**
     * @brief ShaderPack - Shader Pack 数据容器 + 协调器 (对应 Iris ShaderPack.java)
     *
     * **设计模式**: 协调器模式 (Coordinator Pattern)
     *
     * **职责**:
     * 1. **数据容器**: 持有所有子系统（IncludeGraph, ShaderProperties, ProgramSet, Options）
     * 2. **协调器**: 协调子系统初始化顺序和依赖关系
     * 3. **统一访问**: 提供子系统的统一访问接口
     *
     * **架构对比**:
     * - [OLD] **旧设计 (Loader 模式)**: ShaderPackLoader 负责加载 + 持有数据（职责混合）
     * - [NEW] **新设计 (Coordinator 模式)**: ShaderPack 只负责协调子系统，加载逻辑分散到各子系统
     *
     * **子系统职责分离**:
     * - **IncludeGraph**: 管理 #include 依赖图（BFS 构建 + 循环检测）
     * - **ShaderProperties**: 解析 shaders.properties 配置（全局指令、程序指令）
     * - **ShaderPackOptions**: 管理 Shader Pack 选项（基于 ShaderProperties 和 IncludeGraph）
     * - **ProgramSet**: 管理所有着色器程序（47 个单一程序 + 6 个程序数组）
     *
     * **初始化顺序**（构造函数协调）:
     * 1. 扫描起始路径（着色器程序文件）
     * 2. 构建 IncludeGraph（BFS 加载所有依赖）
     * 3. 解析 ShaderProperties（读取配置文件）[WARNING] Phase 2-3 实现
     * 4. 创建 ShaderPackOptions（基于前两者）[WARNING] Phase 5 实现
     * 5. 加载 ProgramSet（基于 IncludeGraph 和 ShaderProperties）[WARNING] Phase 4 实现
     *
     * **Iris 源码参考**:
     * - ShaderPack.java（数据容器 + 协调器）
     * - ProgramSet.java（程序集管理）
     * - IncludeGraph.java（依赖图构建）
     *
     * **Phase 1 实现范围** [IMPORTANT]:
     * - [DONE] 重命名 ShaderPackLoader → ShaderPack
     * - [DONE] 添加子系统成员变量（IncludeGraph, ShaderProperties, ShaderPackOptions, ProgramSet）
     * - [DONE] 重构构造函数（协调器模式）
     * - [DONE] 添加访问器方法
     * - [WARNING] 子系统初始化（IncludeGraph 完整实现，其他子系统为空壳）
     */
    class ShaderPack
    {
    public:
        /**
         * @brief 构造 ShaderPack（协调器模式）
         * @param root Shader Pack 根目录路径
         *
         * **初始化流程**（协调所有子系统）:
         * 1. 扫描起始路径（shaders/ 目录下的所有 .vs.hlsl 和 .ps.hlsl 文件）
         * 2. 构建 IncludeGraph（BFS 加载所有依赖）
         * 3. 解析 ShaderProperties（读取 shaders.properties）[WARNING] Phase 2-3 实现
         * 4. 创建 ShaderPackOptions（基于 IncludeGraph 和 ShaderProperties）[WARNING] Phase 5 实现
         * 5. 加载 ProgramSet（基于 IncludeGraph 和 ShaderProperties）[WARNING] Phase 4 实现
         *
         * **教学要点**:
         * - 协调器模式：构造函数负责协调所有子系统的初始化顺序
         * - 依赖管理：IncludeGraph 先初始化，后续子系统依赖它
         * - 错误处理：任何子系统初始化失败都会记录，但不会中断后续初始化
         *
         * **Phase 1 实施范围**:
         * - [DONE] 扫描起始路径
         * - [DONE] 初始化 IncludeGraph
         * - [WARNING] 其他子系统创建空对象（Phase 2-5 填充实现）
         */
        explicit ShaderPack(const std::filesystem::path& root);

        /**
         * @brief 析构函数（默认实现）
         *
         * **教学要点**:
         * - unique_ptr 自动管理子系统生命周期
         * - RAII 原则：资源在构造函数获取，析构函数释放
         */
        ~ShaderPack() = default;

        // 禁用拷贝（子系统不应该被拷贝）
        ShaderPack(const ShaderPack&)            = delete;
        ShaderPack& operator=(const ShaderPack&) = delete;

        // 支持移动（允许转移所有权）
        ShaderPack(ShaderPack&&) noexcept            = default;
        ShaderPack& operator=(ShaderPack&&) noexcept = default;

        // ========================================================================
        // 子系统访问器 - 提供统一访问接口
        // ========================================================================

        /**
         * @brief 获取 Include 依赖图
         * @return IncludeGraph 引用（保证非空）
         *
         * **使用场景**:
         * - 查询文件依赖关系
         * - 获取已加载的文件节点
         * - 检查循环依赖
         */
        const IncludeGraph& GetIncludeGraph() const { return *m_includeGraph; }

        /**
         * @brief 获取 Shader Pack 配置
         * @return ShaderProperties 引用（保证非空）
         *
         * **使用场景** [WARNING] Phase 2-3 实现:
         * - 查询全局指令（如 shadowMapResolution）
         * - 查询程序指令（如混合模式、深度测试）
         * - 查询 RT 格式配置
         */
        const ShaderProperties& GetShaderProperties() const { return *m_shaderProperties; }

        /**
         * @brief 获取 Shader Pack 选项
         * @return ShaderPackOptions 引用（保证非空）
         *
         * **使用场景** [WARNING] Phase 5 实现:
         * - 查询用户可配置的选项
         * - 处理选项变更事件
         */
        const ShaderPackOptions& GetOptions() const { return *m_options; }

        /**
         * @brief 获取指定维度的 ProgramSet（字符串模式 - 推荐）[RECOMMENDED]
         * @param dimensionName 维度名称字符串（如 "world0", "world1", "world99"）
         * @return ProgramSet 指针，如果不存在则自动创建（延迟加载）
         *
         * 教学要点：
         * - 主要接口，提供最大灵活性
         * - 支持无限维度数量（world0, world1, ..., world99, world-1 等）
         * - 从 App 端传入维度字符串到 RenderSubsystem
         * - 自动处理维度覆盖目录（如 shaders/world1/）
         *
         * Phase 4.4 实现：
         * - Lazy Loading Pattern - 首次访问时才创建
         * - 自动扫描维度覆盖目录（shaders/worldX/）
         * - Fallback 到 world0（如果维度目录不存在）
         */
        ProgramSet* GetProgramSet(const std::string& dimensionName);

        /**
         * @brief 获取默认维度（world0）的 ProgramSet
         * @return 默认 ProgramSet 指针（等价于 GetProgramSet("world0")）
         *
         * 教学要点：
         * - 保留原有单 ProgramSet 的兼容性
         * - 内部调用 GetProgramSet("world0")
         * - 向后兼容 Phase 4.1-4.3 的代码
         */
        ProgramSet* GetProgramSet() { return GetProgramSet("world0"); }

        /**
         * @brief 获取默认维度（world0）的 ProgramSet（const 版本）
         * @return 默认 ProgramSet 指针（等价于 GetProgramSet("world0")）
         */
        const ProgramSet* GetProgramSet() const { return const_cast<ShaderPack*>(this)->GetProgramSet("world0"); }

        // ========================================================================
        // 基本信息
        // ========================================================================

        /**
         * @brief 获取 Shader Pack 根目录
         * @return 根目录路径（文件系统路径）
         */
        const std::filesystem::path& GetRootPath() const { return m_root; }

        /**
         * @brief 检查 Shader Pack 是否有效加载
         * @return true 如果所有子系统都成功初始化
         *
         * **验证逻辑**:
         * - IncludeGraph 加载成功（无循环依赖）
         * - IncludeGraph 没有致命错误
         * - ShaderProperties 解析成功 [WARNING] Phase 2-3
         * - ProgramSet 至少加载了 Basic 程序 [WARNING] Phase 4
         */
        bool IsValid() const;

    private:
        // ========================================================================
        // Phase 4.4 - 维度覆盖系统辅助函数
        // ========================================================================

        /**
         * @brief 检测 Base ProgramSet 的默认程序目录
         * @return 目录名称（"world0", "program", 或 ""）
         *
         * **检测优先级（遵循 Iris 标准）**：
         * 1. **world0/** - 如果存在，使用主世界作为默认 [PRIORITY-HIGH]
         * 2. **program/** - 如果存在，使用专用程序目录 [PRIORITY-MEDIUM]
         * 3. **""** (空字符串) - 回退到 shaders/ 根目录
         *
         * **教学要点**：
         * - Iris 标准：优先使用 world0/ 作为 Base ProgramSet 目录
         * - program/ 是 Iris 标准的默认程序目录（单数）
         * - 如果两者都不存在，回退到 shaders/ 根目录
         * - 这个检测结果影响所有维度的 Fallback Chain 行为
         *
         * **Iris 对应**：
         * - net.irisshaders.iris.pipeline.ProgramSet.scanDirectoryForPrograms()
         * - Iris 也使用相同的目录优先级规则
         */
        std::string DetectBaseProgramDirectory() const;

        /**
         * @brief 加载特定维度的 ProgramSet（延迟加载核心逻辑）
         * @param dimensionName 维度目录名（如 "world1"）
         * @return 新创建的 ProgramSet（如果该维度没有覆盖则返回 world0 的拷贝）
         *
         * 教学要点：
         * - Lazy Loading Pattern - 首次访问时才创建
         * - 自动扫描维度覆盖目录（shaders/worldX/）
         * - Fallback 到 world0（如果维度目录不存在）
         * - 复用 IncludeGraph 节点加载逻辑
         *
         * 实现流程：
         * 1. 构造维度覆盖目录路径（如 "shaders/world1/"）
         * 2. 检查该维度目录是否存在（通过 IncludeGraph）
         * 3. 如果存在，优先搜索维度目录
         * 4. 如果不存在，Fallback 到默认目录（"shaders/"）
         * 5. 加载所有47个单一程序和6个程序数组
         */
        std::unique_ptr<ProgramSet> LoadDimensionProgramSet(const std::string& dimensionName);

        std::filesystem::path m_root; // Shader Pack 根目录

        // ========================================================================
        // 子系统（协调器持有）
        // ========================================================================
        std::unique_ptr<IncludeGraph>      m_includeGraph; // Include 依赖图
        std::unique_ptr<ShaderProperties>  m_shaderProperties; // Shader Pack 配置
        std::unique_ptr<ShaderPackOptions> m_options; // Shader Pack 选项

        // ========================================================================
        // Phase 4.4 - 维度覆盖系统（混合模式）
        // ========================================================================

        // 维度 → ProgramSet 映射（使用字符串键，支持无限维度）
        // Key: 维度名称字符串（"world0", "world1", "world-1", "world99" 等）
        // Value: 该维度的 ProgramSet（延迟加载）
        std::unordered_map<std::string, std::unique_ptr<ProgramSet>> m_programSets;

        // 移除旧的单 ProgramSet 成员（已被 m_programSets 取代）
        // std::unique_ptr<ProgramSet> m_programSet; // [REMOVED] 删除
    };
} // namespace enigma::graphic
