#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <array>

namespace enigma::graphic
{
    /**
     * @class ShaderPackSourceNames
     * @brief HLSL 着色器文件名枚举器（生成所有可能的 .vs.hlsl, .ps.hlsl, .cs.hlsl 等文件名）
     *
     * **设计原则**：
     * - 编译期常量（constexpr + std::array）
     * - 零运行时开销
     * - DirectX 12 HLSL 扩展名约定
     *
     * **扩展名约定**：
     * - `.hlsl` - 库和公共文件（如 `Common.hlsl`, `Lighting.hlsl`）
     * - `.vs.hlsl` - Vertex Shader
     * - `.ps.hlsl` - Pixel Shader (相当于 Fragment Shader)
     * - `.gs.hlsl` - Geometry Shader
     * - `.hs.hlsl` - Hull Shader (相当于 Tessellation Control)
     * - `.ds.hlsl` - Domain Shader (相当于 Tessellation Evaluation)
     * - `.cs.hlsl` - Compute Shader
     *
     * **对应 Iris 源码**：
     * - Iris: ShaderPackSourceNames.java (GLSL 扩展名)
     * - Enigma: ShaderPackSourceNames.hpp (HLSL 扩展名)
     * - 职责：枚举所有可能的着色器源文件名
     *
     * **使用场景**：
     * - IncludeGraph 扫描 Shader Pack 时确定哪些文件需要解析
     * - ShaderPackLoader 查找着色器程序时遍历所有可能的文件名
     * - 文件系统扫描优化（只检查已知扩展名）
     *
     * **示例**：
     * ```cpp
     * auto extensions = ShaderPackSourceNames::GetAllShaderExtensions();
     * // 结果: [".vs.hlsl", ".ps.hlsl", ".gs.hlsl", ".cs.hlsl", ".cs_a.hlsl", ...]
     *
     * auto allNames = ShaderPackSourceNames::GenerateAllPossibleNames("gbuffers_terrain");
     * // 结果: ["gbuffers_terrain.vs.hlsl", "gbuffers_terrain.ps.hlsl", ...]
     *
     * bool isShader = ShaderPackSourceNames::IsShaderSourceFile("gbuffers_terrain.vs.hlsl");
     * // 结果: true
     *
     * bool isLib = ShaderPackSourceNames::IsLibraryFile("Common.hlsl");
     * // 结果: true
     * ```
     */
    class ShaderPackSourceNames
    {
    public:
        // ========================================================================
        // 扩展名常量定义
        // ========================================================================

        /**
         * @brief 标准 HLSL 着色器扩展名（5种）
         *
         * 教学要点：
         * - constexpr std::array 编译期常量，零运行时开销
         * - std::string_view 避免字符串拷贝
         * - DirectX 12 HLSL 扩展名约定
         */
        static constexpr std::array<std::string_view, 5> STANDARD_EXTENSIONS = {
            ".vs.hlsl", // Vertex Shader
            ".hs.hlsl", // Hull Shader (Tessellation Control)
            ".ds.hlsl", // Domain Shader (Tessellation Evaluation)
            ".gs.hlsl", // Geometry Shader
            ".ps.hlsl" // Pixel Shader (Fragment)
        };

        /**
         * @brief Compute 着色器基础扩展名
         */
        static constexpr std::string_view COMPUTE_EXTENSION = ".cs.hlsl";

        /**
         * @brief 库文件扩展名（公共头文件）
         */
        static constexpr std::string_view LIBRARY_EXTENSION = ".hlsl";

        /**
         * @brief Compute 着色器变体数量（包括基础 .cs.hlsl + 26 个字母变体）
         */
        static constexpr int COMPUTE_VARIANT_COUNT = 27;

        // ========================================================================
        // 文件名生成接口
        // ========================================================================

        /**
         * @brief 生成指定基础名的所有可能文件名
         * @param baseName 着色器基础名（如 "gbuffers_terrain"）
         * @param includeComputeVariants 是否包含 Compute 着色器的所有变体（默认 false）
         * @return 所有可能的文件名列表
         *
         * 教学要点：
         * - 动态生成文件名列表
         * - 可选是否包含 Compute 变体（减少不必要的文件名）
         *
         * 示例：
         * - baseName = "gbuffers_terrain", includeComputeVariants = false
         * - 结果: ["gbuffers_terrain.vs.hlsl", "gbuffers_terrain.ps.hlsl", ...]（6个文件名）
         * - baseName = "final", includeComputeVariants = true
         * - 结果: ["final.vs.hlsl", "final.ps.hlsl", ..., "final.cs.hlsl", "final_a.cs.hlsl", ..., "final_z.cs.hlsl"]（32个文件名）
         */
        static std::vector<std::string> GenerateAllPossibleNames(
            const std::string& baseName,
            bool               includeComputeVariants = false
        );

        /**
         * @brief 获取所有 HLSL 着色器扩展名（包括 Compute 变体）
         * @return 所有扩展名列表
         *
         * 教学要点：
         * - 一次性生成所有可能的扩展名
         * - 用于文件系统扫描时快速过滤
         */
        static std::vector<std::string> GetAllShaderExtensions();

        // ========================================================================
        // 文件类型检查
        // ========================================================================

        /**
         * @brief 检查文件名是否是着色器源文件（不包括库文件）
         * @param fileName 文件名（如 "gbuffers_terrain.vs.hlsl"）
         * @return true 如果是已知的着色器文件扩展名
         *
         * 教学要点：
         * - 快速检查文件是否需要被 IncludeGraph 解析
         * - 支持所有 DirectX 12 HLSL 扩展名
         *
         * 示例：
         * - "gbuffers_terrain.vs.hlsl" → true
         * - "shaders.properties" → false
         * - "final_a.cs.hlsl" → true
         * - "Common.hlsl" → false（库文件，使用 IsLibraryFile 检查）
         */
        static bool IsShaderSourceFile(const std::string& fileName);

        /**
         * @brief 检查文件名是否是库文件（.hlsl 纯头文件）
         * @param fileName 文件名（如 "Common.hlsl"）
         * @return true 如果是库文件扩展名
         *
         * 业务逻辑：
         * - "Common.hlsl" → true
         * - "Lighting.hlsl" → true
         * - "gbuffers_terrain.vs.hlsl" → false（着色器文件）
         */
        static bool IsLibraryFile(const std::string& fileName);

        /**
         * @brief 检查扩展名是否是 Compute 着色器变体
         * @param extension 扩展名（如 ".cs.hlsl", ".cs_a.hlsl"）
         * @return true 如果是 Compute 着色器扩展名
         */
        static bool IsComputeShaderExtension(const std::string& extension);

        // ========================================================================
        // Compute 着色器变体生成
        // ========================================================================

        /**
         * @brief 生成指定基础名的所有 Compute 着色器变体文件名
         * @param baseName 着色器基础名（如 "final"）
         * @return Compute 着色器变体文件名列表（27个）
         *
         * 业务逻辑：
         * - final.cs.hlsl（基础变体）
         * - final_a.cs.hlsl, final_b.cs.hlsl, ..., final_z.cs.hlsl（26个字母变体）
         *
         * 教学要点：
         * - 对应 Iris 的 addComputeStarts() 方法
         * - 支持多个 Compute 着色器并行执行（Iris 特性）
         */
        static std::vector<std::string> GenerateComputeVariantNames(const std::string& baseName);

    private:
        /**
         * @brief 提取文件扩展名（包括 .）
         * @param fileName 文件名
         * @return 扩展名（如 ".vs.hlsl"），如果无扩展名返回空字符串
         *
         * 教学要点：简单的字符串解析辅助函数
         */
        static std::string GetFileExtension(const std::string& fileName);

        /**
         * 教学要点总结：
         * 1. **编译期常量优化**：constexpr std::array 零运行时开销
         * 2. **std::string_view 应用**：避免字符串拷贝，提升性能
         * 3. **DirectX 12 HLSL 扩展名**：.vs.hlsl, .ps.hlsl, .cs.hlsl 等
         * 4. **库文件支持**：.hlsl 纯头文件（如 Common.hlsl）
         * 5. **Compute 着色器变体支持**：27个变体（.cs.hlsl + 26个字母）
         * 6. **快速文件过滤**：IsShaderSourceFile() 和 IsLibraryFile() 快速检查文件类型
         */
    };
} // namespace enigma::graphic
