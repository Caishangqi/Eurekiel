#include "ShaderPackLoader.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>

namespace enigma::graphic
{
    // ========================================================================
    // 静态映射表初始化
    // ========================================================================

    // ProgramId 名称映射表
    const std::unordered_map<std::string, ShaderPackLoader::ProgramId> ShaderPackLoader::s_ProgramIdMap = {
        // 核心渲染程序
        {"gbuffers_basic", ProgramId::GBUFFERS_BASIC},
        {"gbuffers_textured", ProgramId::GBUFFERS_TEXTURED},
        {"gbuffers_textured_lit", ProgramId::GBUFFERS_TEXTURED_LIT},
        {"gbuffers_skybasic", ProgramId::GBUFFERS_SKYBASIC},
        {"gbuffers_skytextured", ProgramId::GBUFFERS_SKYTEXTURED},
        {"gbuffers_clouds", ProgramId::GBUFFERS_CLOUDS},
        {"gbuffers_terrain", ProgramId::GBUFFERS_TERRAIN},
        {"gbuffers_terrain_solid", ProgramId::GBUFFERS_TERRAIN_SOLID},
        {"gbuffers_terrain_cutout", ProgramId::GBUFFERS_TERRAIN_CUTOUT},
        {"gbuffers_damagedblock", ProgramId::GBUFFERS_DAMAGEDBLOCK},
        {"gbuffers_block", ProgramId::GBUFFERS_BLOCK},
        {"gbuffers_beaconbeam", ProgramId::GBUFFERS_BEACONBEAM},
        {"gbuffers_item", ProgramId::GBUFFERS_ITEM},
        {"gbuffers_entities", ProgramId::GBUFFERS_ENTITIES},
        {"gbuffers_armor_glint", ProgramId::GBUFFERS_ARMOR_GLINT},
        {"gbuffers_spidereyes", ProgramId::GBUFFERS_SPIDEREYES},
        {"gbuffers_hand", ProgramId::GBUFFERS_HAND},
        {"gbuffers_weather", ProgramId::GBUFFERS_WEATHER},
        {"gbuffers_water", ProgramId::GBUFFERS_WATER},
        // 最终合成
        {"final", ProgramId::FINAL},
        // 阴影
        {"shadow", ProgramId::SHADOW},
        {"shadow_solid", ProgramId::SHADOW_SOLID},
        {"shadow_cutout", ProgramId::SHADOW_CUTOUT},
    };

    // ProgramId 文件名映射表
    const std::unordered_map<ShaderPackLoader::ProgramId, std::string> ShaderPackLoader::s_ProgramFileNameMap = {
        {ProgramId::GBUFFERS_BASIC, "gbuffers_basic"},
        {ProgramId::GBUFFERS_TEXTURED, "gbuffers_textured"},
        {ProgramId::GBUFFERS_TEXTURED_LIT, "gbuffers_textured_lit"},
        {ProgramId::GBUFFERS_SKYBASIC, "gbuffers_skybasic"},
        {ProgramId::GBUFFERS_SKYTEXTURED, "gbuffers_skytextured"},
        {ProgramId::GBUFFERS_CLOUDS, "gbuffers_clouds"},
        {ProgramId::GBUFFERS_TERRAIN, "gbuffers_terrain"},
        {ProgramId::GBUFFERS_TERRAIN_SOLID, "gbuffers_terrain_solid"},
        {ProgramId::GBUFFERS_TERRAIN_CUTOUT, "gbuffers_terrain_cutout"},
        {ProgramId::GBUFFERS_DAMAGEDBLOCK, "gbuffers_damagedblock"},
        {ProgramId::GBUFFERS_BLOCK, "gbuffers_block"},
        {ProgramId::GBUFFERS_BEACONBEAM, "gbuffers_beaconbeam"},
        {ProgramId::GBUFFERS_ITEM, "gbuffers_item"},
        {ProgramId::GBUFFERS_ENTITIES, "gbuffers_entities"},
        {ProgramId::GBUFFERS_ARMOR_GLINT, "gbuffers_armor_glint"},
        {ProgramId::GBUFFERS_SPIDEREYES, "gbuffers_spidereyes"},
        {ProgramId::GBUFFERS_HAND, "gbuffers_hand"},
        {ProgramId::GBUFFERS_WEATHER, "gbuffers_weather"},
        {ProgramId::GBUFFERS_WATER, "gbuffers_water"},
        {ProgramId::FINAL, "final"},
        {ProgramId::SHADOW, "shadow"},
        {ProgramId::SHADOW_SOLID, "shadow_solid"},
        {ProgramId::SHADOW_CUTOUT, "shadow_cutout"},
    };

    // ProgramArrayId 名称映射表
    const std::unordered_map<std::string, ShaderPackLoader::ProgramArrayId> ShaderPackLoader::s_ProgramArrayIdMap = {
        {"deferred", ProgramArrayId::DEFERRED},
        {"composite", ProgramArrayId::COMPOSITE},
    };

    // ProgramArrayId 文件名映射表
    const std::unordered_map<ShaderPackLoader::ProgramArrayId, std::string> ShaderPackLoader::s_ProgramArrayFileNameMap = {
        {ProgramArrayId::DEFERRED, "deferred"},
        {ProgramArrayId::COMPOSITE, "composite"},
    };

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
        m_ArrayPrograms[ProgramArrayId::DEFERRED]  = {};
        m_ArrayPrograms[ProgramArrayId::COMPOSITE] = {};

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

    ShaderPackLoader::ProgramId ShaderPackLoader::ParseProgramId(const std::string& name)
    {
        auto it = s_ProgramIdMap.find(name);
        if (it != s_ProgramIdMap.end())
        {
            return it->second;
        }
        return ProgramId::COUNT;
    }

    ShaderPackLoader::ProgramArrayId ShaderPackLoader::ParseProgramArrayId(const std::string& name)
    {
        auto it = s_ProgramArrayIdMap.find(name);
        if (it != s_ProgramArrayIdMap.end())
        {
            return it->second;
        }
        return ProgramArrayId::COUNT;
    }

    std::string ShaderPackLoader::GetProgramFileName(ProgramId id)
    {
        auto it = s_ProgramFileNameMap.find(id);
        if (it != s_ProgramFileNameMap.end())
        {
            return it->second;
        }
        return "";
    }

    std::string ShaderPackLoader::GetProgramArrayFileName(ProgramArrayId arrayId)
    {
        auto it = s_ProgramArrayFileNameMap.find(arrayId);
        if (it != s_ProgramArrayFileNameMap.end())
        {
            return it->second;
        }
        return "";
    }

    std::optional<ShaderPackLoader::ShaderFile> ShaderPackLoader::FindWithFallback(
        ProgramId id, const std::filesystem::path& searchDir) const
    {
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

    std::vector<ShaderPackLoader::ProgramId> ShaderPackLoader::GetFallbackChain(ProgramId id)
    {
        // Iris Fallback Chain 定义 (简化版)
        switch (id)
        {
        case ProgramId::GBUFFERS_TERRAIN:
        case ProgramId::GBUFFERS_TERRAIN_SOLID:
        case ProgramId::GBUFFERS_TERRAIN_CUTOUT:
            return {ProgramId::GBUFFERS_TEXTURED, ProgramId::GBUFFERS_BASIC};

        case ProgramId::GBUFFERS_WATER:
            return {ProgramId::GBUFFERS_TERRAIN, ProgramId::GBUFFERS_TEXTURED, ProgramId::GBUFFERS_BASIC};

        case ProgramId::GBUFFERS_BLOCK:
        case ProgramId::GBUFFERS_BEACONBEAM:
        case ProgramId::GBUFFERS_ITEM:
        case ProgramId::GBUFFERS_ENTITIES:
        case ProgramId::GBUFFERS_ARMOR_GLINT:
        case ProgramId::GBUFFERS_SPIDEREYES:
        case ProgramId::GBUFFERS_HAND:
        case ProgramId::GBUFFERS_WEATHER:
            return {ProgramId::GBUFFERS_TEXTURED, ProgramId::GBUFFERS_BASIC};

        case ProgramId::GBUFFERS_SKYTEXTURED:
            return {ProgramId::GBUFFERS_SKYBASIC, ProgramId::GBUFFERS_BASIC};

        case ProgramId::GBUFFERS_CLOUDS:
            return {ProgramId::GBUFFERS_SKYBASIC, ProgramId::GBUFFERS_BASIC};

        case ProgramId::SHADOW_SOLID:
        case ProgramId::SHADOW_CUTOUT:
            return {ProgramId::SHADOW};

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
