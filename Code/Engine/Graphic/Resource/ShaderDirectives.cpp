/**
 * @file ShaderDirectives.cpp
 * @brief Iris 着色器注释指令解析器实现
 * @date 2025-10-03
 *
 * 职责:
 * - 从 HLSL 源码注释中提取 Iris 风格配置指令
 * - 解析 RENDERTARGETS, DRAWBUFFERS, BLEND 等渲染状态
 * - 解析 const 常量定义和 RT 格式配置
 *
 * 教学要点:
 * - 正则表达式解析注释语法
 * - 字符串解析和分词
 * - 枚举值映射 (字符串 → DXGI_FORMAT)
 */

#include "ShaderDirectives.hpp"
#include <regex>
#include <sstream>
#include <algorithm>

namespace enigma::graphic
{
    /**
     * @brief 辅助函数: 去除字符串首尾空白
     */
    static std::string Trim(const std::string& str)
    {
        size_t first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos)
            return "";
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, (last - first + 1));
    }

    /**
     * @brief 辅助函数: 字符串转大写
     */
    static std::string ToUpper(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return str;
    }

    /**
     * @brief 辅助函数: 解析逗号分隔的整数列表
     * @param str "0,1,2,3"
     * @return {0, 1, 2, 3}
     */
    static std::vector<uint32_t> ParseIntList(const std::string& str)
    {
        std::vector<uint32_t> result;
        std::stringstream     ss(str);
        std::string           token;

        while (std::getline(ss, token, ','))
        {
            token = Trim(token);
            if (!token.empty())
            {
                result.push_back(static_cast<uint32_t>(std::stoi(token)));
            }
        }

        return result;
    }

    /**
     * @brief 辅助函数: 字符串转 DXGI_FORMAT
     * @param str "DXGI_FORMAT_R16G16B16A16_FLOAT"
     * @return DXGI_FORMAT 枚举值
     */
    static DXGI_FORMAT ParseDXGIFormat(const std::string& str)
    {
        std::string upper = ToUpper(Trim(str));

        // 常见格式映射表
        if (upper == "DXGI_FORMAT_R16G16B16A16_FLOAT") return DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (upper == "DXGI_FORMAT_R32G32B32A32_FLOAT") return DXGI_FORMAT_R32G32B32A32_FLOAT;
        if (upper == "DXGI_FORMAT_R8G8B8A8_UNORM") return DXGI_FORMAT_R8G8B8A8_UNORM;
        if (upper == "DXGI_FORMAT_R8G8B8A8_UNORM_SRGB") return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        if (upper == "DXGI_FORMAT_R16G16_FLOAT") return DXGI_FORMAT_R16G16_FLOAT;
        if (upper == "DXGI_FORMAT_R32G32_FLOAT") return DXGI_FORMAT_R32G32_FLOAT;
        if (upper == "DXGI_FORMAT_R11G11B10_FLOAT") return DXGI_FORMAT_R11G11B10_FLOAT;
        if (upper == "DXGI_FORMAT_R10G10B10A2_UNORM") return DXGI_FORMAT_R10G10B10A2_UNORM;

        // 默认格式
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    /**
     * @brief 重置所有字段
     */
    void ShaderDirectives::Reset()
    {
        renderTargets.clear();
        drawBuffers.clear();
        rtFormats.clear();
        rtSizes.clear();
        blendMode.reset();
        depthTest.reset();
        depthWrite.reset();
        cullFace.reset();
        computeThreads.reset();
        computeSize.reset();
        customDefines.clear();
    }

    /**
     * @brief 从 HLSL 源码解析 Iris 指令
     * @param source HLSL 源代码字符串
     * @return 解析后的 ShaderDirectives
     *
     * 支持的注释格式:
     * 1. RENDERTARGETS: 0,1,2
     * 2. DRAWBUFFERS: 0123
     * 3. BLEND: SrcAlpha OneMinusSrcAlpha
     * 4. DEPTHTEST: LessEqual
     * 5. DEPTHWRITE: false
     * 6. CULLFACE: Back
     * 7. COMPUTE_THREADS: 16,16,1
     * 8. const int colortex0Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
     * 9. const vec2 gaux1Size = vec2(0.5, 0.5);
     */
    ShaderDirectives ShaderDirectives::Parse(const std::string& source)
    {
        ShaderDirectives directives;

        // ========================================================================
        // 1. 解析块注释指令 /* KEYWORD: value */
        // ========================================================================

        std::regex                  blockCommentRegex(R"(/\*\s*([A-Z_]+)\s*:\s*([^*]+)\*/)");
        std::smatch                 match;
        std::string::const_iterator searchStart(source.cbegin());

        while (std::regex_search(searchStart, source.cend(), match, blockCommentRegex))
        {
            std::string keyword = ToUpper(Trim(match[1].str()));
            std::string value   = Trim(match[2].str());

            if (keyword == "RENDERTARGETS")
            {
                directives.renderTargets = ParseIntList(value);
            }
            else if (keyword == "DRAWBUFFERS")
            {
                directives.drawBuffers = value;
            }
            else if (keyword == "BLEND")
            {
                directives.blendMode = value;
            }
            else if (keyword == "DEPTHTEST")
            {
                directives.depthTest = value;
            }
            else if (keyword == "DEPTHWRITE")
            {
                std::string lowerValue = ToUpper(value);
                directives.depthWrite  = (lowerValue == "TRUE" || lowerValue == "1");
            }
            else if (keyword == "CULLFACE")
            {
                directives.cullFace = value;
            }
            else if (keyword == "COMPUTE_THREADS")
            {
                auto nums = ParseIntList(value);
                if (nums.size() == 3)
                {
                    directives.computeThreads = std::make_tuple(nums[0], nums[1], nums[2]);
                }
            }
            else if (keyword == "COMPUTE_SIZE")
            {
                auto nums = ParseIntList(value);
                if (nums.size() == 3)
                {
                    directives.computeSize = std::make_tuple(nums[0], nums[1], nums[2]);
                }
            }

            searchStart = match.suffix().first;
        }

        // ========================================================================
        // 2. 解析行注释 const 定义 // const type name = value;
        // ========================================================================

        std::regex constRegex(R"(//\s*const\s+(\w+)\s+(\w+)\s*=\s*([^;]+);)");
        searchStart = source.cbegin();

        while (std::regex_search(searchStart, source.cend(), match, constRegex))
        {
            std::string type  = Trim(match[1].str());
            std::string name  = Trim(match[2].str());
            std::string value = Trim(match[3].str());

            // 检查是否是 RT 格式定义 (colortex0Format, colortex1Format, ...)
            if (name.find("Format") != std::string::npos)
            {
                directives.rtFormats[name] = ParseDXGIFormat(value);
            }
            // 检查是否是 RT 尺寸定义 (gaux1Size, gaux2Size, ...)
            else if (name.find("Size") != std::string::npos)
            {
                // 解析 vec2(0.5, 0.5) 格式
                std::regex  vec2Regex(R"(vec2\s*\(\s*([\d.]+)\s*,\s*([\d.]+)\s*\))");
                std::smatch vec2Match;
                if (std::regex_search(value, vec2Match, vec2Regex))
                {
                    float x                  = std::stof(vec2Match[1].str());
                    float y                  = std::stof(vec2Match[2].str());
                    directives.rtSizes[name] = {x, y};
                }
            }
            else
            {
                // 其他自定义常量
                directives.customDefines[name] = value;
            }

            searchStart = match.suffix().first;
        }

        return directives;
    }
} // namespace enigma::graphic
