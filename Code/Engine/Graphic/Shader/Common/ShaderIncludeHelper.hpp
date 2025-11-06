/**
 * @file ShaderIncludeHelper.hpp
 * @brief Shader Include系统辅助工具类 - 统一管理Include系统相关工具函数
 * @date 2025-11-04
 * @author Caizii
 *
 * 架构说明:
 * ShaderIncludeHelper 是一个纯静态工具类，为Shader Include系统提供便捷的工具函数接口。
 * 它整合了路径处理、Include图构建和Include展开等常用功能，简化Include系统的使用。
 *
 * 设计原则:
 * - 纯静态类: 所有函数都是static，禁止实例化
 * - 统一接口: 为Include系统提供一致的工具函数访问方式
 * - 便捷封装: 简化常用操作的实现复杂度
 * - 功能分组: 按功能域组织函数（路径处理、图构建、展开处理）
 *
 * 职责边界:
 * - + 路径处理工具 (DetermineRootPath, IsPathWithinRoot, NormalizePath, ResolveRelativePath)
 * - + Include图构建便捷接口 (BuildFromFiles, BuildFromShaderPack)
 * - + Include展开便捷接口 (ExpandShaderSource)
 * - ❌ 不负责具体的文件IO实现 (委托给IFileReader)
 * - ❌ 不负责Include图的算法实现 (委托给IncludeGraph)
 * - ❌ 不负责Include展开算法实现 (委托给IncludeProcessor)
 *
 * 使用示例:
 * @code
 * // ========== 路径处理示例 ==========
 * 
 * // 1. 从任意路径推断根目录
 * std::filesystem::path root = ShaderIncludeHelper::DetermineRootPath("F:/shaderpacks/MyPack/shaders/main.hlsl");
 * // root = "F:/shaderpacks/MyPack"
 * 
 * // 2. 检查路径安全性
 * bool safe = ShaderIncludeHelper::IsPathWithinRoot("F:/shaderpacks/MyPack/shaders/lib/common.hlsl", "F:/shaderpacks/MyPack");
 * // safe = true
 * 
 * // 3. 标准化路径字符串
 * std::string normalized = ShaderIncludeHelper::NormalizePath("shaders\\..\\lib\\common.hlsl");
 * // normalized = "shaders/lib/common.hlsl"
 * 
 * // 4. 解析相对路径
 * ShaderPath basePath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.hlsl");
 * ShaderPath includePath = ShaderIncludeHelper::ResolveRelativePath(basePath, "../lib/common.hlsl");
 * // includePath = "/shaders/lib/common.hlsl"
 * 
 * // ========== Include图构建示例 ==========
 * 
 * // 5. 从文件系统构建Include图
 * std::vector<std::string> shaderFiles = {
 *     "shaders/gbuffers_terrain.vs.hlsl",
 *     "shaders/gbuffers_terrain.ps.hlsl"
 * };
 * auto graph1 = ShaderIncludeHelper::BuildFromFiles("F:/shaderpacks/MyPack", shaderFiles);
 * 
 * // 6. 从ShaderPack构建Include图
 * std::vector<ShaderPath> shaderPaths = {
 *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl"),
 *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.ps.hlsl")
 * };
 * auto graph2 = ShaderIncludeHelper::BuildFromShaderPack("F:/shaderpacks/MyPack", shaderPaths);
 * 
 * // ========== Include展开示例 ==========
 * 
 * // 7. 展开着色器源码
 * if (graph2 && !graph2->GetFailures().empty()) {
 *     ShaderPath mainPath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl");
 *     std::string expanded = ShaderIncludeHelper::ExpandShaderSource(*graph2, mainPath);
 *     
 *     // 或者包含行号指令（用于调试）
 *     std::string expandedWithLines = ShaderIncludeHelper::ExpandShaderSource(*graph2, mainPath, true);
 * }
 * 
 * // ========== 完整使用流程示例 ==========
 * 
 * // 完整流程：从Shader Pack目录构建到代码展开
 * std::string packRoot = "F:/shaderpacks/MyPack";
 * std::vector<std::string> programFiles = {
 *     "shaders/composite.fsh.hlsl",
 *     "shaders/gbuffers_basic.vs.hlsl",
 *     "shaders/gbuffers_basic.ps.hlsl"
 * };
 * 
 * // 构建Include图
 * auto includeGraph = ShaderIncludeHelper::BuildFromFiles(packRoot, programFiles);
 * if (!includeGraph) {
 *     // 处理构建失败
 *     return;
 * }
 * 
 * // 检查构建失败
 * if (!includeGraph->GetFailures().empty()) {
 *     for (const auto& [path, error] : includeGraph->GetFailures()) {
 *         std::cerr << "Include error: " << path.GetPathString() << " - " << error << std::endl;
 *     }
 * }
 * 
 * // 展开所有程序
 * for (const auto& file : programFiles) {
 *     ShaderPath shaderPath = ShaderPath::FromAbsolutePath("/" + file);
 *     if (includeGraph->HasNode(shaderPath)) {
 *         std::string expandedCode = ShaderIncludeHelper::ExpandShaderSource(*includeGraph, shaderPath);
 *         // 使用展开的代码进行编译...
 *     }
 * }
 * @endcode
 *
 * 教学要点:
 * - 工具类模式 (Utility Class Pattern)
 * - 静态方法设计 (Static Method Design)
 * - 便捷函数封装 (Convenience Function Wrapper)
 * - 路径处理最佳实践 (Path Handling Best Practices)
 * - Include系统架构 (Include System Architecture)
 */

#pragma once

#include "Engine/Graphic/Shader/ShaderPack/Include/IncludeGraph.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Include/IncludeProcessor.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Include/ShaderPath.hpp"
#include "Engine/Graphic/Shader/Common/FileSystemReader.hpp"
#include "Engine/Graphic/Shader/ShaderPack/ShaderPackReader.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <memory>

namespace enigma::graphic
{
    /**
     * @class ShaderIncludeHelper
     * @brief Shader Include系统辅助工具类 - 纯静态工具类
     *
     * 设计理念:
     * - 纯静态类：所有方法都是static，禁止实例化
     * - 功能分组：按职责域组织（路径处理、图构建、展开处理）
     * - 便捷封装：简化Include系统的常用操作
     * - 统一接口：为Include系统提供一致的工具函数访问方式
     *
     * 功能分组:
     * 1. **路径处理函数**：
     *    - DetermineRootPath: 从任意路径自动推断根目录
     *    - IsPathWithinRoot: 检查路径是否在根目录内
     *    - NormalizePath: 标准化路径字符串
     *    - ResolveRelativePath: 解析相对路径
     *
     * 2. **Include图构建便捷接口**：
     *    - BuildFromFiles: 从文件系统构建Include图
     *    - BuildFromShaderPack: 从ShaderPack构建Include图
     *
     * 3. **Include展开便捷接口**：
     *    - ExpandShaderSource: 展开shader源代码（支持可选的line directives）
     *
     * 教学要点:
     * - 工具类模式 (Utility Class Pattern)
     * - 静态方法设计 (Static Method Design)
     * - 依赖注入原则 (Dependency Injection Principle)
     * - 门面模式 (Facade Pattern)
     * - RAII资源管理
     */
    class ShaderIncludeHelper
    {
    public:
        // 禁止实例化 (纯静态工具类)
        ShaderIncludeHelper()                                      = delete;
        ~ShaderIncludeHelper()                                     = delete;
        ShaderIncludeHelper(const ShaderIncludeHelper&)            = delete;
        ShaderIncludeHelper& operator=(const ShaderIncludeHelper&) = delete;

        // ========================================================================
        // 路径处理函数组
        // ========================================================================

        /**
         * @brief 从任意路径自动推断根目录
         * @param anyPath 任意路径（文件路径或目录路径）
         * @return std::filesystem::path 推断出的根目录
         *
         * 教学要点:
         * - 路径推断算法：从文件路径向上查找合理的根目录
         * - 常见目录结构识别：shaders/, src/, assets/ 等
         * - 容错处理：无法识别时返回父目录
         *
         * 推断策略:
         * 1. 如果路径指向文件 → 使用文件的父目录
         * 2. 查找常见的项目根目录标识：
         *    - shaders/ 目录存在 → 其父目录作为根目录
         *    - src/ 目录存在 → 其父目录作为根目录
         *    - assets/ 目录存在 → 其父目录作为根目录
         * 3. 如果无法识别 → 返回路径的父目录
         *
         * 示例:
         * @code
         * // 从文件路径推断
         * auto root1 = ShaderIncludeHelper::DetermineRootPath("F:/shaderpacks/MyPack/shaders/main.hlsl");
         * // root1 = "F:/shaderpacks/MyPack"
         *
         * // 从目录路径推断
         * auto root2 = ShaderIncludeHelper::DetermineRootPath("F:/shaderpacks/MyPack/shaders");
         * // root2 = "F:/shaderpacks/MyPack"
         *
         * // 从深层路径推断
         * auto root3 = ShaderIncludeHelper::DetermineRootPath("F:/project/src/shaders/lib/common.hlsl");
         * // root3 = "F:/project/src"
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 这是一个启发式算法，可能不准确
         * - ⚠️ 对于精确控制，建议直接指定根目录
         * - ⚠️ 路径必须存在才能正确推断
         */
        static std::filesystem::path DetermineRootPath(const std::filesystem::path& anyPath);

        /**
         * @brief 检查路径是否在根目录内
         * @param path 要检查的路径
         * @param root 根目录路径
         * @return bool 在根目录内返回true，否则返回false
         *
         * 教学要点:
         * - 路径安全检查：防止路径遍历攻击
         * - 规范化路径处理：使用std::filesystem的规范化功能
         * - 符号链接安全：处理符号链接可能的安全��险
         *
         * 检查策略:
         * 1. 将两个路径都转换为绝对和规范形式
         * 2. 检查path是否以root开头
         * 3. 确保不越过root边界
         * 4. 处理符号链接的安全风险
         *
         * 示例:
         * @code
         * std::filesystem::path root = "F:/shaderpacks/MyPack";
         * 
         * // 安全路径
         * bool safe1 = ShaderIncludeHelper::IsPathWithinRoot("F:/shaderpacks/MyPack/shaders/lib/common.hlsl", root);
         * // safe1 = true
         *
         * // 危险路径（路径遍历攻击）
         * bool safe2 = ShaderIncludeHelper::IsPathWithinRoot("F:/shaderpacks/MyPack/../../../etc/passwd", root);
         * // safe2 = false
         * @endcode
         *
         * 安全特性:
         * - 防止路径遍历攻击 (../etc/passwd)
         * - 处理符号链接安全风险
         * - 跨平台路径处理
         */
        static bool IsPathWithinRoot(
            const std::filesystem::path& path,
            const std::filesystem::path& root
        );

        /**
         * @brief 标准化路径字符串
         * @param path 原始路径字符串
         * @return std::string 标准化后的路径字符串
         *
         * 教学要点:
         * - 路径标准化：统一路径分隔符和处理相对路径
         * - 跨平台兼容：将Windows反斜杠转换为Unix正斜杠
         * - 相对路径处理：解析 . 和 .. 组件
         *
         * 标准化规则:
         * 1. 统一使用 / 作为路径分隔符
         * 2. 解析 . (当前目录) 组件
         * 3. 解析 .. (上级目录) 组件
         * 4. 移除多余的连续分隔符
         * 5. 移除末尾的分隔符（除��是根路径）
         *
         * 示例:
         * @code
         * // Windows路径标准化
         * std::string norm1 = ShaderIncludeHelper::NormalizePath("shaders\\lib\\common.hlsl");
         * // norm1 = "shaders/lib/common.hlsl"
         *
         * // 相对路径处理
         * std::string norm2 = ShaderIncludeHelper::NormalizePath("shaders/./lib/../common.hlsl");
         * // norm2 = "shaders/common.hlsl"
         *
         * // 多余分隔符处理
         * std::string norm3 = ShaderIncludeHelper::NormalizePath("shaders//lib///common.hlsl");
         * // norm3 = "shaders/lib/common.hlsl"
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 这是一个字符串级别的标准化，不访问文件系统
         * - ⚠️ 不会验证路径是否存在
         * - ⚠️ 不会解析符号链接
         */
        static std::string NormalizePath(const std::string& path);

        /**
         * @brief 解析相对路径
         * @param basePath 基础路径（Shader Pack内绝对路径）
         * @param relativePath 相对路径字符串
         * @return ShaderPath 解析后的绝对路径
         *
         * 教学要点:
         * - 相对路径解析：基于基础路径解析相对路径
         * - ShaderPath封装：返回ShaderPath对象而非字符串
         * - Include指令处理：主要用于处理#include指令中的路径
         *
         * 解析规则:
         * - 如果relativePath以/开头 → 直接作为绝对路径
         * - 否则 → 基于basePath解析相对路径
         * - 使用ShaderPath::Resolve()进行实际解析
         *
         * 示例:
         * @code
         * ShaderPath basePath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.hlsl");
         *
         * // 相对路径解析
         * ShaderPath path1 = ShaderIncludeHelper::ResolveRelativePath(basePath, "../lib/common.hlsl");
         * // path1 = "/shaders/lib/common.hlsl"
         *
         * // 绝对路径（以/开头）
         * ShaderPath path2 = ShaderIncludeHelper::ResolveRelativePath(basePath, "/engine/Common.hlsl");
         * // path2 = "/engine/Common.hlsl"
         *
         * // 同级目录
         * ShaderPath path3 = ShaderIncludeHelper::ResolveRelativePath(basePath, "lighting.glsl");
         * // path3 = "/shaders/lighting.glsl"
         * @endcode
         *
         * 使用场景:
         * - Include指令解析 (#include "../lib/common.hlsl")
         * - 文件路径计算
         * - 资源定位
         */
        static ShaderPath ResolveRelativePath(
            const ShaderPath&  basePath,
            const std::string& relativePath
        );

        // ========================================================================
        // Include图构建便捷接口
        // ========================================================================

        /**
         * @brief 从文件系统构建Include图
         * @param anyPath 任意路径（用于推断Shader Pack根目录）
         * @param relativeShaderPaths 相对着色器路径列表
         * @return std::unique_ptr<IncludeGraph> 构建的Include图（失败返回nullptr）
         *
         * 教学要点:
         * - 便捷接口：简化从文件系统构建Include图的流程
         * - 自动推断：自动推断Shader Pack根目录
         * - 错误处理：构建失败时返回nullptr
         *
         * 构建流程:
         * 1. 使用DetermineRootPath()推断Shader Pack根目录
         * 2. 将相对路径转换为ShaderPath对象
         * 3. 创建FileSystemReader并构建IncludeGraph
         * 4. 返回构建的Include图（失败返回nullptr）
         *
         * 示例:
         * @code
         * std::vector<std::string> shaderFiles = {
         *     "shaders/gbuffers_terrain.vs.hlsl",
         *     "shaders/gbuffers_terrain.ps.hlsl",
         *     "shaders/lib/common.hlsl"
         * };
         *
         * auto graph = ShaderIncludeHelper::BuildFromFiles("F:/shaderpacks/MyPack", shaderFiles);
         * if (graph) {
         *     std::cout << "Include graph built successfully" << std::endl;
         *     std::cout << "Nodes: " << graph->GetNodes().size() << std::endl;
         * } else {
         *     std::cout << "Failed to build include graph" << std::endl;
         * }
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 构建失败时返回nullptr，调用者需要检查
         * - ⚠️ 可能抛出std::runtime_error（循环依赖）
         * - ⚠️ 使用相对路径基于推断的根目录
         */
        static std::unique_ptr<IncludeGraph> BuildFromFiles(
            const std::filesystem::path&    anyPath,
            const std::vector<std::string>& relativeShaderPaths
        );

        /**
         * @brief 从ShaderPack构建Include图
         * @param packRoot Shader Pack根目录（文件系统路径）
         * @param shaderPaths 着色器路径列表（ShaderPath对象）
         * @return std::unique_ptr<IncludeGraph> 构建的Include图（失败返回nullptr）
         *
         * 教学要点:
         * - 精确控制：明确指定Shader Pack根目录
         * - ShaderPath接口：使用ShaderPath对象而非字符串
         * - ShaderPackReader：使用专门的Shader Pack文件读取器
         *
         * 构建流程:
         * 1. 创建ShaderPackReader（基于packRoot）
         * 2. 使用ShaderPackReader构建IncludeGraph
         * 3. 返回构建的Include图（失败返回nullptr）
         *
         * 示例:
         * @code
         * std::filesystem::path packRoot = "F:/shaderpacks/MyPack";
         * std::vector<ShaderPath> shaderPaths = {
         *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl"),
         *     ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.ps.hlsl"),
         *     ShaderPath::FromAbsolutePath("/shaders/lib/common.hlsl")
         * };
         *
         * auto graph = ShaderIncludeHelper::BuildFromShaderPack(packRoot, shaderPaths);
         * if (graph) {
         *     // 检查构建失败
         *     if (!graph->GetFailures().empty()) {
         *         for (const auto& [path, error] : graph->GetFailures()) {
         *             std::cerr << "Include error: " << path.GetPathString() << " - " << error << std::endl;
         *         }
         *     }
         * }
         * @endcode
         *
         * 优势:
         * - 精确的根目录控制
         * - 使用ShaderPackReader优化性能
         * - 支持Shader Pack特殊文件访问逻辑
         *
         * 注意事项:
         * - ⚠️ 构建失败时返回nullptr，调用者需要检查
         * - ⚠️ 可能抛出std::runtime_error（循环依赖）
         * - ⚠️ packRoot必须指向有效的Shader Pack目录
         */
        static std::unique_ptr<IncludeGraph> BuildFromShaderPack(
            const std::filesystem::path&   packRoot,
            const std::vector<ShaderPath>& shaderPaths
        );

        // ========================================================================
        // Include展开便捷接口
        // ========================================================================

        /**
         * @brief 展开Shader源代码的所有#include指令
         * @param graph 已构建的Include依赖图
         * @param shaderPath 要展开的着色器路径
         * @param withLineDirectives 是否包含行号指令（默认false）
         * @return std::string 展开后的完整着色器代码
         *
         * 教学要点:
         * - Include展开：将所有#include指令替换为实际文件内容
         * - 行号指令：可选地插入#line指令用于调试
         * - 便捷接口：简化IncludeProcessor的使用
         *
         * 展开流程:
         * 1. 验证shaderPath在IncludeGraph中存在
         * 2. 根据withLineDirectives选择展开方式：
         *    - false → 使用IncludeProcessor::Expand()
         *    - true → 使用IncludeProcessor::ExpandWithLineDirectives()
         * 3. 返回展开后的代码
         *
         * HLSL #line指令格式:
         * - `#line <line_number> "<file_path>"`
         * - 用于DXC编译错误定位到原始文件
         *
         * 示例:
         * @code
         * auto graph = ShaderIncludeHelper::BuildFromFiles(packRoot, shaderFiles);
         * if (graph) {
         *     ShaderPath mainPath = ShaderPath::FromAbsolutePath("/shaders/gbuffers_terrain.vs.hlsl");
         *     
         *     // 简单展开（发布版本）
         *     std::string expanded = ShaderIncludeHelper::ExpandShaderSource(*graph, mainPath);
         *     
         *     // 包含行号指令（开发版本）
         *     std::string expandedWithLines = ShaderIncludeHelper::ExpandShaderSource(*graph, mainPath, true);
         * }
         * @endcode
         *
         * 输入示例:
         * ```hlsl
         * // gbuffers_terrain.vs.hlsl
         * #include "Common.hlsl"
         * #include "lib/Lighting.hlsl"
         *
         * void main() {
         *     // shader code here
         * }
         * ```
         *
         * 输出示例（withLineDirectives=true）:
         * ```hlsl
         * #line 1 "/shaders/Common.hlsl"
         * // Common.hlsl 的内容...
         * #line 1 "/shaders/lib/Lighting.hlsl"
         * // Lighting.hlsl 的内容...
         * #line 3 "/shaders/gbuffers_terrain.vs.hlsl"
         *
         * void main() {
         *     // shader code here
         * }
         * ```
         *
         * 异常:
         * - std::invalid_argument: 如果shaderPath不在IncludeGraph中
         *
         * 使用建议:
         * - 开发阶段：使用withLineDirectives=true（更好的错误定位）
         * - 发布版本：使用withLineDirectives=false（更小的文件大小）
         *
         * 注意事项:
         * - ⚠️ 需要有效的IncludeGraph（调用者确保graph非空）
         * - ⚠️ shaderPath必须在IncludeGraph中存在
         * - ⚠️ 大型着色器可能导致展开后代码很大
         */
        static std::string ExpandShaderSource(
            const IncludeGraph& graph,
            const ShaderPath&   shaderPath,
            bool                withLineDirectives = false
        );

    private:
        /**
         * 教学要点总结：
         * 1. **工具类模式**：纯静态类，所有方法都是static，禁止实例化
         * 2. **功能分组设计**：按职责域组织（路径处理、图构建、展开处理）
         * 3. **便捷接口封装**：简化常用操作的实现复杂度
         * 4. **路径安全处理**：防止路径遍历攻击，确保文件访问安全
         * 5. **依赖注入原则**：接受依赖而非创建依赖，提高可测试性
         * 6. **错误处理策略**：使用返回值而非异常处理预期错误情况
         * 7. **跨平台兼容**：使用std::filesystem处理平台差异
         * 8. **RAII资源管理**：使用智能指针管理动态资源
         */
    };
} // namespace enigma::graphic
