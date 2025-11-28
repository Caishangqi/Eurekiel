#include "ShaderPackLoader.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

#include "Engine/Core/EngineCommon.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 公共方法实现
    // ========================================================================

    bool ShaderPackLoader::LoadShaderPack(const std::filesystem::path& shaderPackPath)
    {
        m_RootPath   = shaderPackPath;
        m_ShadersDir = shaderPackPath / "shaders";

        if (!std::filesystem::exists(m_ShadersDir))
        {
            return false;
        }

        // 扫描单一程序和程序数组
        ScanShaderDirectory(m_ShadersDir);

        // 扫描程序数组 (composite0-99, deferred0-99)
        for (auto& [arrayId, programs] : m_ArrayPrograms)
        {
            ScanProgramArray(m_ShadersDir, arrayId, programs);
        }

        // 检测维度覆盖
        DetectDimensionOverrides();

        m_IsLoaded = true;
        return true;
    }

    ShaderPackLoader::ShaderFile ShaderPackLoader::GetShaderFile(
        ProgramId id, const std::string& dimensionId) const
    {
        // 先查找维度特定的着色器
        for (const auto& dimOverride : m_DimensionOverrides)
        {
            if (dimOverride.dimensionId == dimensionId)
            {
                auto result = FindWithFallback(id, dimOverride.shaderDir);
                if (result)
                {
                    return *result;
                }
            }
        }

        // 回退到默认 shaders/ 目录
        auto result = FindWithFallback(id, m_ShadersDir);
        if (result)
        {
            return *result;
        }

        // 如果都找不到,返回空的 ShaderFile
        return ShaderFile{};
    }

    std::array<ShaderPackLoader::ShaderFile, 100> ShaderPackLoader::GetShaderFileArray(
        ProgramArrayId arrayId, const std::string& dimensionId) const
    {
        // 先查找维度特定的着色器数组
        for (const auto& dimOverride : m_DimensionOverrides)
        {
            if (dimOverride.dimensionId == dimensionId)
            {
                // TODO: 实现维度覆盖的程序数组查找
                // 当前简化版本: 仅返回默认数组
            }
        }

        // 返回默认数组
        auto it = m_ArrayPrograms.find(arrayId);
        if (it != m_ArrayPrograms.end())
        {
            return it->second;
        }

        // 返回空数组
        return std::array<ShaderFile, 100>{};
    }

    // ========================================================================
    // 私有方法实现
    // ========================================================================

    void ShaderPackLoader::ScanShaderDirectory(const std::filesystem::path& shadersDir)
    {
        if (!std::filesystem::exists(shadersDir))
        {
            return;
        }

        // 初始化程序数组
        m_ArrayPrograms[ProgramArrayId::Deferred]  = {};
        m_ArrayPrograms[ProgramArrayId::Composite] = {};

        for (const auto& entry : std::filesystem::directory_iterator(shadersDir))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }

            const auto& path     = entry.path();
            const auto  filename = path.filename().string();

            // 移除扩展名: composite1.vs.hlsl -> composite1
            std::string stem = path.stem().stem().string();

            // 检测是否为程序数组 (composite1, deferred2, etc.)
            std::regex  arrayPattern(R"(^(composite|deferred)(\d*)$)");
            std::smatch match;
            if (std::regex_match(stem, match, arrayPattern))
            {
                // 这是程序数组,跳过 (稍后由 ScanProgramArray 处理)
                continue;
            }

            // 检测是否为顶点着色器 (.vs.hlsl) - C++17兼容写法
            if (filename.size() >= 8 && filename.substr(filename.size() - 8) == ".vs.hlsl")
            {
                auto id = ParseProgramId(stem);
                if (id != ProgramId::COUNT)
                {
                    m_SinglePrograms[id].vertexPath = path;
                }
            }
            // 检测是否为像素着色器 (.ps.hlsl) - DirectX 术语
            else if (filename.size() >= 8 && filename.substr(filename.size() - 8) == ".ps.hlsl")
            {
                auto id = ParseProgramId(stem);
                if (id != ProgramId::COUNT)
                {
                    m_SinglePrograms[id].pixelPath = path; // DirectX 术语: Pixel Shader
                }
            }
        }
    }

    void ShaderPackLoader::ScanProgramArray(
        const std::filesystem::path& shadersDir,
        ProgramArrayId               arrayId,
        std::array<ShaderFile, 100>& programs)
    {
        std::string baseName = GetProgramArrayFileName(arrayId);

        // Iris 实现: ProgramSet.java:166-174
        // 循环 0-99 尝试加载 name, name1, name2...name99
        for (int i = 0; i < 100; ++i)
        {
            std::string suffix   = (i == 0) ? "" : std::to_string(i);
            std::string filename = baseName + suffix;

            // 尝试加载顶点着色器
            auto vsPath = shadersDir / (filename + ".vs.hlsl");
            if (std::filesystem::exists(vsPath))
            {
                programs[i].vertexPath = vsPath;
            }

            // 尝试加载像素着色器 (.ps.hlsl) - DirectX 术语
            auto psPath = shadersDir / (filename + ".ps.hlsl");
            if (std::filesystem::exists(psPath))
            {
                programs[i].pixelPath = psPath; // DirectX 术语: Pixel Shader
            }
        }
    }

    void ShaderPackLoader::DetectDimensionOverrides()
    {
        const std::vector<std::string> dimensionNames = {"world0", "world-1", "world1"};

        for (const auto& dimName : dimensionNames)
        {
            auto dimPath = m_RootPath / "shaders" / dimName;
            if (std::filesystem::exists(dimPath))
            {
                DimensionOverride override;
                override.dimensionId = dimName;
                override.shaderDir   = dimPath;
                m_DimensionOverrides.push_back(override);
            }
        }
    }

    ProgramId ShaderPackLoader::ParseProgramId(const std::string& name)
    {
        // 遍历所有 ProgramId 枚举值,找到匹配的文件名
        for (int i = 0; i < static_cast<int>(ProgramId::COUNT); ++i)
        {
            ProgramId id = static_cast<ProgramId>(i);
            if (ProgramIdToSourceName(id) == name)
            {
                return id;
            }
        }
        return ProgramId::COUNT;
    }

    ProgramArrayId ShaderPackLoader::ParseProgramArrayId(const std::string& name)
    {
        // 使用 StringToProgramArrayId 函数 (来自 ProgramArrayId.hpp)
        return StringToProgramArrayId(name);
    }

    std::string ShaderPackLoader::GetProgramFileName(ProgramId id)
    {
        // 使用 ProgramIdToSourceName 函数 (来自 ProgramId.hpp)
        return ProgramIdToSourceName(id);
    }

    std::string ShaderPackLoader::GetProgramArrayFileName(ProgramArrayId arrayId)
    {
        // 使用 GetProgramArrayPrefix 函数 (来自 ProgramArrayId.hpp)
        return GetProgramArrayPrefix(arrayId);
    }

    std::optional<ShaderPackLoader::ShaderFile> ShaderPackLoader::FindWithFallback(
        ProgramId id, const std::filesystem::path& searchDir) const
    {
        UNUSED(searchDir)
        // 先在已扫描的缓存中查找
        auto it = m_SinglePrograms.find(id);
        if (it != m_SinglePrograms.end())
        {
            return it->second;
        }

        // 使用 Fallback Chain
        auto fallbackChain = GetFallbackChain(id);
        for (auto fallbackId : fallbackChain)
        {
            auto fallbackIt = m_SinglePrograms.find(fallbackId);
            if (fallbackIt != m_SinglePrograms.end())
            {
                return fallbackIt->second;
            }
        }

        return std::nullopt;
    }

    std::vector<ProgramId> ShaderPackLoader::GetFallbackChain(ProgramId id)
    {
        // Iris Fallback Chain 定义 (简化版)
        switch (id)
        {
        case ProgramId::Terrain:
        case ProgramId::TerrainSolid:
        case ProgramId::TerrainCutout:
            return {ProgramId::Textured, ProgramId::Basic};

        case ProgramId::Water:
            return {ProgramId::Terrain, ProgramId::Textured, ProgramId::Basic};

        case ProgramId::Block:
        case ProgramId::BeaconBeam:
        case ProgramId::Item:
        case ProgramId::Entities:
        case ProgramId::ArmorGlint:
        case ProgramId::SpiderEyes:
        case ProgramId::Hand:
        case ProgramId::Weather:
            return {ProgramId::Textured, ProgramId::Basic};

        case ProgramId::SkyTextured:
            return {ProgramId::SkyBasic, ProgramId::Basic};

        case ProgramId::Clouds:
            return {ProgramId::SkyBasic, ProgramId::Basic};

        case ProgramId::ShadowSolid:
        case ProgramId::ShadowCutout:
            return {ProgramId::Shadow};

        default:
            return {};
        }
    }

    std::string ShaderPackLoader::ReadShaderSource(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            return "";
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
} // namespace enigma::graphic
