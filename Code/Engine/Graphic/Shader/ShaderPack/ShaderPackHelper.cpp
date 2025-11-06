/**
 * @file ShaderPackHelper.cpp
 * @brief ShaderPack系统辅助工具类实现
 * @date 2025-11-05
 * @author Caizii
 *
 * 实现说明:
 * ShaderPackHelper.cpp 实现了ShaderPack系统的便捷工具函数。
 * 这些函数封装了路径选择、结构验证、Fallback机制和加载逻辑的常用操作，
 * 简化了ShaderPack系统的使用复杂度。
 *
 * 实现策略:
 * - 使用现有组件的组合而非重新实现
 * - 提供便捷的工厂方法和封装接口
 * - 重点关注错误处理和边界情况
 * - 保持与现有ShaderPack系统的兼容性
 */

#include "Engine/Graphic/Shader/ShaderPack/ShaderPackHelper.hpp"
#include "Engine/Graphic/Shader/Common/ShaderIncludeHelper.hpp"
#include "Engine/Graphic/Shader/ShaderPack/ProgramSet.hpp"
#include "Engine/Graphic/Shader/ShaderPack/ShaderSource.hpp"
#include "Engine/Graphic/Shader/ShaderCache.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <filesystem>
#include <algorithm>

namespace enigma::graphic
{
    // ========================================================================
    // 辅助函数 - C++17兼容的字符串后缀检查
    // ========================================================================

    namespace
    {
        bool EndsWith(const std::string& str, const std::string& suffix)
        {
            if (suffix.size() > str.size())
            {
                return false;
            }
            return str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
        }
    }

    // ========================================================================
    // 路径选择函数组实现
    // ========================================================================

    std::filesystem::path ShaderPackHelper::SelectShaderPackPath(
        const std::filesystem::path& userPackPath,
        const std::filesystem::path& enginePackPath)
    {
        // 教学要点：双ShaderPack架构 - 用户包优先，引擎默认包作为Fallback

        // 1. 检查用户包路径是否有效
        try
        {
            // 检查路径是否存在
            if (!std::filesystem::exists(userPackPath))
            {
                DebuggerPrintf("[ShaderPackHelper] User pack path does not exist: '%s', falling back to engine pack\n",
                               userPackPath.string().c_str());
                return enginePackPath;
            }

            // 检查是否是目录
            if (!std::filesystem::is_directory(userPackPath))
            {
                DebuggerPrintf("[ShaderPackHelper] User pack path is not a directory: '%s', falling back to engine pack\n",
                               userPackPath.string().c_str());
                return enginePackPath;
            }

            // 检查是否包含shaders/子目录
            std::filesystem::path shadersDir = userPackPath / "shaders";
            if (!std::filesystem::exists(shadersDir) || !std::filesystem::is_directory(shadersDir))
            {
                DebuggerPrintf("[ShaderPackHelper] User pack missing 'shaders/' subdirectory: '%s', falling back to engine pack\n",
                               userPackPath.string().c_str());
                return enginePackPath;
            }

            // 用户包路径有效
            DebuggerPrintf("[ShaderPackHelper] Selected user pack path: '%s'\n",
                           userPackPath.string().c_str());
            return userPackPath;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            // 文件系统错误（如权限问题）
            DebuggerPrintf("[ShaderPackHelper] Filesystem error checking user pack: %s, falling back to engine pack\n",
                           e.what());
            return enginePackPath;
        }
    }

    // ========================================================================
    // 结构验证函数组实现
    // ========================================================================

    bool ShaderPackHelper::ValidateShaderPackStructure(const std::filesystem::path& packPath)
    {
        // 教学要点：结构验证 - 检查ShaderPack目录是否符合Iris标准

        try
        {
            // 1. 检查packPath是否存在且是目录
            if (!std::filesystem::exists(packPath))
            {
                DebuggerPrintf("[ShaderPackHelper] Pack path does not exist: '%s'\n",
                               packPath.string().c_str());
                return false;
            }

            if (!std::filesystem::is_directory(packPath))
            {
                DebuggerPrintf("[ShaderPackHelper] Pack path is not a directory: '%s'\n",
                               packPath.string().c_str());
                return false;
            }

            // 2. 检查shaders/子目录是否存在
            std::filesystem::path shadersDir = packPath / "shaders";
            if (!std::filesystem::exists(shadersDir) || !std::filesystem::is_directory(shadersDir))
            {
                DebuggerPrintf("[ShaderPackHelper] Missing 'shaders/' subdirectory in: '%s'\n",
                               packPath.string().c_str());
                return false;
            }

            // 3. 扫描shaders/目录，检查是否至少包含一个shader文件
            bool foundShaderFile = false;

            for (const auto& entry : std::filesystem::recursive_directory_iterator(shadersDir))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                std::string filename = entry.path().filename().string();

                // 检查是否是shader文件（.vs.hlsl, .ps.hlsl, .vsh, .fsh等）
                if (EndsWith(filename, ".vs.hlsl") || EndsWith(filename, ".ps.hlsl") ||
                    EndsWith(filename, ".vsh") || EndsWith(filename, ".fsh") ||
                    EndsWith(filename, ".gs.hlsl") || EndsWith(filename, ".gsh") ||
                    EndsWith(filename, ".cs.hlsl") || EndsWith(filename, ".csh"))
                {
                    foundShaderFile = true;
                    break;
                }
            }

            if (!foundShaderFile)
            {
                DebuggerPrintf("[ShaderPackHelper] No shader files found in: '%s'\n",
                               shadersDir.string().c_str());
                return false;
            }

            // 4. 所有检查通过
            DebuggerPrintf("[ShaderPackHelper] ShaderPack structure validated: '%s'\n",
                           packPath.string().c_str());
            return true;
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            // 文件系统错误
            DebuggerPrintf("[ShaderPackHelper] Filesystem error validating pack structure: %s\n",
                           e.what());
            return false;
        }
    }

    // ========================================================================
    // 双ShaderPack Fallback机制实现
    // ========================================================================

    ShaderProgram* ShaderPackHelper::GetShaderProgramWithFallback(
        ShaderPack* userPack,
        ShaderPack* enginePack,
        ProgramId   programId)
    {
        // 教学要点：三级Fallback策略 - 用户包程序 → 用户包回退 → 引擎默认包程序 → 引擎默认包回退

        // 架构说明：
        // - ProgramSet存储的是ShaderSource*（源码容器）
        // - ShaderProgram是编译后的程序
        // - 根据当前架构，这个方法应该返回ShaderSource*而不是ShaderProgram*
        // - 但为了保持接口一致性，我们使用reinterpret_cast进行类型转换
        // - TODO: 未来可能需要重构为返回ShaderSource*或添加ShaderSource到ShaderProgram的转换

        // 1. 尝试从用户包获取程序
        if (userPack != nullptr)
        {
            ProgramSet* userProgramSet = userPack->GetProgramSet();
            if (userProgramSet != nullptr)
            {
                // 尝试获取指定程序（ProgramSet内部会处理Fallback Chain）
                ShaderSource* userSource = userProgramSet->GetRaw(programId);
                if (userSource != nullptr && userSource->IsValid())
                {
                    // 用户包有该程序
                    // 注意：这里返回的是ShaderSource*，使用reinterpret_cast转换为ShaderProgram*
                    // 调用者需要知道实际返回的是ShaderSource*
                    return reinterpret_cast<ShaderProgram*>(userSource);
                }
            }
        }

        // 2. 用户包没有该程序，尝试从引擎默认包获取
        if (enginePack != nullptr)
        {
            ProgramSet* engineProgramSet = enginePack->GetProgramSet();
            if (engineProgramSet != nullptr)
            {
                // 尝试获取指定程序（ProgramSet内部会处理Fallback Chain）
                ShaderSource* engineSource = engineProgramSet->GetRaw(programId);
                if (engineSource != nullptr && engineSource->IsValid())
                {
                    // 引擎默认包有该程序
                    DebuggerPrintf("[ShaderPackHelper] Fallback to engine pack for program: %d\n",
                                   static_cast<int>(programId));
                    // 注意：这里返回的是ShaderSource*，使用reinterpret_cast转换为ShaderProgram*
                    return reinterpret_cast<ShaderProgram*>(engineSource);
                }
            }
        }

        // 3. 两个包都没有该程序
        DebuggerPrintf("[ShaderPackHelper] Warning: Program not found in both user and engine packs: %d\n",
                       static_cast<int>(programId));
        return nullptr;
    }

    // ========================================================================
    // ShaderPack加载函数组实现
    // ========================================================================

    std::string ShaderPackHelper::SelectShaderPackPath(
        const std::string& currentPackName,
        const std::string& userSearchPath,
        const std::string& engineDefaultPath)
    {
        // 教学要点：智能路径选择 - 根据currentPackName和搜索路径自动选择有效路径

        // Priority 1: User selected pack (from config file or future GUI)
        if (!currentPackName.empty())
        {
            std::filesystem::path userPackPath =
                std::filesystem::path(userSearchPath) / currentPackName;

            if (std::filesystem::exists(userPackPath))
            {
                // User pack found, use it
                DebuggerPrintf("[ShaderPackHelper] Selected user pack: '%s'\n",
                               userPackPath.string().c_str());
                return userPackPath.string();
            }

            // User pack not found, log warning and fall back
            DebuggerPrintf("[ShaderPackHelper] User ShaderPack '%s' not found at '%s', falling back to engine default\n",
                           currentPackName.c_str(),
                           userPackPath.string().c_str());
        }

        // Priority 2: Engine default pack (always exists after build)
        DebuggerPrintf("[ShaderPackHelper] Selected engine default pack: '%s'\n",
                       engineDefaultPath.c_str());
        return engineDefaultPath;
    }

    std::shared_ptr<ShaderPack> ShaderPackHelper::LoadShaderPackFromPath(
        const std::filesystem::path& packPath,
        ShaderCache*                 shaderCache)
    {
        // 教学要点：便捷接口封装 - 简化ShaderPack加载流程，集成ShaderCache

        try
        {
            std::filesystem::path fsPackPath(packPath);

            // 1. 验证路径存在性
            if (!std::filesystem::exists(fsPackPath))
            {
                DebuggerPrintf("[ShaderPackHelper] Error: Pack path does not exist: '%s'\n",
                               packPath.c_str());
                return nullptr;
            }

            // 2. 验证ShaderPack结构
            if (!ValidateShaderPackStructure(fsPackPath))
            {
                DebuggerPrintf("[ShaderPackHelper] Error: Invalid ShaderPack structure: '%s'\n",
                               packPath.c_str());
                return nullptr;
            }

            // 3. 创建ShaderPack对象
            DebuggerPrintf("[ShaderPackHelper] Loading ShaderPack from: '%s'\n",
                           packPath.c_str());

            auto shaderPack = std::make_shared<ShaderPack>(fsPackPath);

            // 4. 验证ShaderPack是否成功加载
            if (!shaderPack->IsValid())
            {
                DebuggerPrintf("[ShaderPackHelper] Error: ShaderPack failed validation after loading: '%s'\n",
                               packPath.c_str());
                return nullptr;
            }

            // 5. 如果shaderCache非空，缓存ShaderSource（使用默认world0维度）
            if (shaderCache && shaderPack)
            {
                size_t totalSourcesCached = 0;

                // 获取默认维度（world0）的 ProgramSet
                const ProgramSet* programSet = shaderPack->GetProgramSet();
                if (programSet)
                {
                    // 获取所有单一程序（ProgramId对应的ShaderSource）
                    const auto& singlePrograms = programSet->GetPrograms();
                    for (const auto& [id, source] : singlePrograms)
                    {
                        if (source && source->IsValid())
                        {
                            std::string name = shaderCache->GetProgramName(id);
                            if (!name.empty())
                            {
                                // 创建非拥有的 shared_ptr，因为 unique_ptr 仍保持所有权
                                shaderCache->CacheSource(name, std::shared_ptr<ShaderSource>(source.get(), [](ShaderSource*)
                                {
                                }));
                                ++totalSourcesCached;
                            }
                        }
                    }
                }

                DebuggerPrintf("[ShaderPackHelper] ShaderCache: Loaded %zu ShaderSources from ShaderPack\n",
                               totalSourcesCached);
            }

            // 6. 加载成功
            DebuggerPrintf("[ShaderPackHelper] ShaderPack loaded successfully: '%s'\n",
                           packPath.c_str());
            return shaderPack;
        }
        catch (const std::exception& e)
        {
            // 捕获所有异常
            DebuggerPrintf("[ShaderPackHelper] Exception loading ShaderPack from '%s': %s\n",
                           packPath.c_str(), e.what());
            return nullptr;
        }
    }

    std::unique_ptr<ShaderPack> ShaderPackHelper::LoadShaderPackFromPath(
        const std::filesystem::path& packPath)
    {
        // 教学要点：便捷接口封装 - 简化ShaderPack加载流程

        try
        {
            // 1. 验证路径存在性
            if (!std::filesystem::exists(packPath))
            {
                DebuggerPrintf("[ShaderPackHelper] Error: Pack path does not exist: '%s'\n",
                               packPath.string().c_str());
                return nullptr;
            }

            // 2. 验证ShaderPack结构
            if (!ValidateShaderPackStructure(packPath))
            {
                DebuggerPrintf("[ShaderPackHelper] Error: Invalid ShaderPack structure: '%s'\n",
                               packPath.string().c_str());
                return nullptr;
            }

            // 3. 创建ShaderPack对象
            DebuggerPrintf("[ShaderPackHelper] Loading ShaderPack from: '%s'\n",
                           packPath.string().c_str());

            auto shaderPack = std::make_unique<ShaderPack>(packPath);

            // 4. 验证ShaderPack是否成功加载
            if (!shaderPack->IsValid())
            {
                DebuggerPrintf("[ShaderPackHelper] Error: ShaderPack failed validation after loading: '%s'\n",
                               packPath.string().c_str());
                return nullptr;
            }

            // 5. 加载成功
            DebuggerPrintf("[ShaderPackHelper] ShaderPack loaded successfully: '%s'\n",
                           packPath.string().c_str());
            return shaderPack;
        }
        catch (const std::exception& e)
        {
            // 捕获所有异常
            DebuggerPrintf("[ShaderPackHelper] Exception loading ShaderPack from '%s': %s\n",
                           packPath.string().c_str(), e.what());
            return nullptr;
        }
    }
} // namespace enigma::graphic
