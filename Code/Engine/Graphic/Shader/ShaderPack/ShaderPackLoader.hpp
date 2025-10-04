#pragma once

#include <string>
#include <vector>
#include <array>
#include <optional>
#include <unordered_map>
#include <memory>
#include <filesystem>

namespace enigma::graphic
{
    /**
     * @brief ShaderPackLoader - Shader Pack 加载器 (Iris 风格)
     *
     * 教学目标:
     * 1. 加载 Shader Pack 目录结构 (shaders/, world0/, world-1/, world1/)
     * 2. 实现 Fallback Chain 自动回退机制
     * 3. 支持维度覆盖系统 (Dimension Override)
     * 4. 支持动态数量的程序数组 (composite0-99, deferred0-99)
     *
     * 架构设计原则:
     * - **独立子系统**: 不依赖引擎 ResourceProvider,直接文件系统访问
     * - **KISS 原则**: 简化 Iris 的复杂配置系统,专注核心加载功能
     * - **一次性加载**: 启动时加载完毕,无热重载开销
     *
     * Iris 源码参考:
     * - ProgramSet.java:163-175 (readProgramArray - 100个槽位预分配)
     * - ProgramArrayId.java (DEFERRED, COMPOSITE 等数组类型)
     * - ProgramId.java (单一程序类型)
     */
    class ShaderPackLoader
    {
    public:
        /**
         * @brief 单一着色器程序类型枚举
         *
         * Iris 设计: ProgramId (单一程序) vs ProgramArrayId (程序数组)
         * 参考: ProgramId.java
         *
         * 这个枚举只包含单一程序 (不支持数字后缀)
         */
        enum class ProgramId
        {
            // 核心渲染程序 (优先级最高)
            GBUFFERS_BASIC, // 最基础的几何着色器
            GBUFFERS_TEXTURED, // 基础纹理着色器
            GBUFFERS_TEXTURED_LIT, // 光照纹理着色器
            GBUFFERS_SKYBASIC, // 天空盒基础
            GBUFFERS_SKYTEXTURED, // 天空盒纹理
            GBUFFERS_CLOUDS, // 云层
            GBUFFERS_TERRAIN, // 地形 (固体+透明)
            GBUFFERS_TERRAIN_SOLID, // 固体地形
            GBUFFERS_TERRAIN_CUTOUT, // 剪裁地形 (草、树叶)
            GBUFFERS_DAMAGEDBLOCK, // 破坏动画
            GBUFFERS_BLOCK, // 方块实体
            GBUFFERS_BEACONBEAM, // 信标光束
            GBUFFERS_ITEM, // 物品
            GBUFFERS_ENTITIES, // 实体
            GBUFFERS_ARMOR_GLINT, // 附魔光泽
            GBUFFERS_SPIDEREYES, // 蜘蛛眼睛
            GBUFFERS_HAND, // 手部
            GBUFFERS_WEATHER, // 天气 (雨雪)
            GBUFFERS_WATER, // 水

            // 最终合成
            FINAL,

            // 阴影
            SHADOW,
            SHADOW_SOLID,
            SHADOW_CUTOUT,

            COUNT // 总数
        };

        /**
         * @brief 程序数组类型枚举
         *
         * Iris 设计: 支持动态数量的着色器程序 (0-99)
         * 例如: composite, composite1, composite2 ... composite99
         * 参考: ProgramArrayId.java
         *
         * 每个数组类型支持最多100个程序槽位
         */
        enum class ProgramArrayId
        {
            DEFERRED, // deferred, deferred1...deferred99
            COMPOSITE, // composite, composite1...composite99

            COUNT // 总数
        };

        /**
         * @brief 着色器文件信息
         */
        struct ShaderFile
        {
            std::filesystem::path vertexPath; // 顶点着色器路径
            std::filesystem::path fragmentPath; // 片段着色器路径
            std::string           vertexSource; // HLSL 源码 (加载后缓存)
            std::string           fragmentSource; // HLSL 源码

            bool hasVertex() const { return !vertexPath.empty(); }
            bool hasFragment() const { return !fragmentPath.empty(); }
        };

        /**
         * @brief 维度覆盖信息
         *
         * Iris 支持不同维度使用不同着色器
         * - world0: 主世界 (Overworld)
         * - world-1: 下界 (Nether)
         * - world1: 末地 (End)
         */
        struct DimensionOverride
        {
            std::string           dimensionId; // "world0", "world-1", "world1"
            std::filesystem::path shaderDir; // 维度着色器目录
        };

    public:
        ShaderPackLoader()  = default;
        ~ShaderPackLoader() = default;

        /**
         * @brief 加载 Shader Pack
         *
         * 从指定目录加载着色器包,实现完整的 Fallback Chain
         *
         * 加载流程 (Iris ShaderPack.java:95-150):
         * 1. 扫描 shaders/ 目录
         * 2. 检测维度覆盖目录 (world0/, world-1/, world1/)
         * 3. 加载单一程序 (ProgramId)
         * 4. 加载程序数组 (ProgramArrayId, 0-99 槽位)
         * 5. 构建 Fallback Chain
         *
         * @param shaderPackPath Shader Pack 根目录路径
         * @return 如果加载成功返回 true
         */
        bool LoadShaderPack(const std::filesystem::path& shaderPackPath);

        /**
         * @brief 获取单一程序的着色器文件
         *
         * 实现 Fallback Chain 自动回退:
         * 例如: gbuffers_terrain → gbuffers_textured → gbuffers_basic
         *
         * @param id 程序ID
         * @param dimensionId 维度ID (可选,默认 "world0")
         * @return 如果找到返回着色器文件,否则返回空的 ShaderFile
         */
        ShaderFile GetShaderFile(ProgramId id, const std::string& dimensionId = "world0") const;

        /**
         * @brief 获取程序数组的着色器文件
         *
         * 支持动态数量的着色器程序 (0-99)
         * 例如: composite, composite1, composite2 ... composite99
         *
         * Iris 实现参考: ProgramSet.java:163-175
         * - 预分配100个槽位数组
         * - 槽位0: composite.vs.hlsl/ps.hlsl
         * - 槽位1: composite1.vs.hlsl/ps.hlsl
         * - ...
         * - 不存在的程序返回空 ShaderFile (hasVertex/hasFragment 返回 false)
         *
         * @param arrayId 程序数组ID
         * @param dimensionId 维度ID (可选,默认 "world0")
         * @return 返回数组,包含最多100个程序 (不存在的程序为空 ShaderFile)
         */
        std::array<ShaderFile, 100> GetShaderFileArray(ProgramArrayId arrayId, const std::string& dimensionId = "world0") const;

        /**
         * @brief 检查是否加载成功
         */
        bool IsLoaded() const { return m_IsLoaded; }

        /**
         * @brief 获取 Shader Pack 根目录
         */
        const std::filesystem::path& GetRootPath() const { return m_RootPath; }

    private:
        /**
         * @brief 扫描 shaders/ 目录,查找所有着色器文件
         *
         * HLSL 文件命名约定:
         * - 顶点着色器: *.vs.hlsl (例: gbuffers_terrain.vs.hlsl)
         * - 像素着色器: *.ps.hlsl (例: gbuffers_terrain.ps.hlsl)
         * - 程序数组: composite.vs.hlsl, composite1.vs.hlsl, composite2.vs.hlsl...
         *
         * 实现提示:
         * 1. 遍历目录查找 *.vs.hlsl 和 *.ps.hlsl 文件
         * 2. 单一程序按照 ProgramId 分类
         * 3. 数组程序按照 ProgramArrayId + 索引分类
         * 4. 缓存文件路径到 m_SinglePrograms 和 m_ArrayPrograms
         */
        void ScanShaderDirectory(const std::filesystem::path& shadersDir);

        /**
         * @brief 扫描程序数组文件
         *
         * Iris 实现参考: ProgramSet.java:166-174
         * 循环 0-99 尝试加载 name, name1, name2...name99
         *
         * 示例:
         * - composite.vs.hlsl → 索引 0
         * - composite1.vs.hlsl → 索引 1
         * - composite2.vs.hlsl → 索引 2
         * - ...
         * - composite99.vs.hlsl → 索引 99
         *
         * @param shadersDir 着色器目录
         * @param arrayId 程序数组ID
         * @param programs 输出数组 (100个槽位)
         */
        void ScanProgramArray(
            const std::filesystem::path& shadersDir,
            ProgramArrayId               arrayId,
            std::array<ShaderFile, 100>& programs);

        /**
         * @brief 检测维度覆盖目录
         *
         * Iris 实现参考: ShaderPack.java:135-147
         * 检测 world0/, world-1/, world1/ 目录是否存在
         */
        void DetectDimensionOverrides();

        /**
         * @brief 程序ID名称到枚举的映射
         *
         * 示例:
         * "gbuffers_terrain" → ProgramId::GBUFFERS_TERRAIN
         * "shadow_solid" → ProgramId::SHADOW_SOLID
         */
        static ProgramId ParseProgramId(const std::string& name);

        /**
         * @brief 程序数组ID名称到枚举的映射
         *
         * 示例:
         * "composite" → ProgramArrayId::COMPOSITE
         * "deferred" → ProgramArrayId::DEFERRED
         */
        static ProgramArrayId ParseProgramArrayId(const std::string& name);

        /**
         * @brief 程序ID枚举到文件名前缀
         *
         * 示例:
         * ProgramId::GBUFFERS_TERRAIN → "gbuffers_terrain"
         */
        static std::string GetProgramFileName(ProgramId id);

        /**
         * @brief 程序数组ID枚举到文件名前缀
         *
         * 示例:
         * ProgramArrayId::COMPOSITE → "composite"
         */
        static std::string GetProgramArrayFileName(ProgramArrayId arrayId);

        /**
         * @brief Fallback Chain 查找 (单一程序)
         *
         * 实现 Iris 的回退逻辑:
         * gbuffers_terrain → gbuffers_textured → gbuffers_basic
         *
         * Iris 参考: ProgramFallback.java
         */
        std::optional<ShaderFile> FindWithFallback(ProgramId id, const std::filesystem::path& searchDir) const;

        /**
         * @brief 获取程序ID的 Fallback 链
         *
         * 示例:
         * GBUFFERS_TERRAIN → [GBUFFERS_TEXTURED, GBUFFERS_BASIC]
         * GBUFFERS_WATER → [GBUFFERS_TERRAIN, GBUFFERS_TEXTURED, GBUFFERS_BASIC]
         */
        static std::vector<ProgramId> GetFallbackChain(ProgramId id);

        /**
         * @brief 读取着色器文件内容
         *
         * 使用 std::filesystem 读取 HLSL 源码
         */
        static std::string ReadShaderSource(const std::filesystem::path& path);

    private:
        bool                  m_IsLoaded = false;
        std::filesystem::path m_RootPath;
        std::filesystem::path m_ShadersDir; // shaders/ 目录

        // 单一程序映射 (ProgramId → ShaderFile)
        std::unordered_map<ProgramId, ShaderFile> m_SinglePrograms;

        // 程序数组映射 (ProgramArrayId → ShaderFile[100])
        std::unordered_map<ProgramArrayId, std::array<ShaderFile, 100>> m_ArrayPrograms;

        // 维度覆盖映射
        std::vector<DimensionOverride> m_DimensionOverrides;

        // 程序名称映射表 (静态初始化)
        static const std::unordered_map<std::string, ProgramId>      s_ProgramIdMap;
        static const std::unordered_map<std::string, ProgramArrayId> s_ProgramArrayIdMap;
        static const std::unordered_map<ProgramId, std::string>      s_ProgramFileNameMap;
        static const std::unordered_map<ProgramArrayId, std::string> s_ProgramArrayFileNameMap;
    };
} // namespace enigma::graphic
