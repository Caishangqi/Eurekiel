/**
 * @file ShaderCache.cpp
 * @brief Unified shader cache management system implementation
 * @date 2025-11-05
 * @author Caizii
 *
 * Implementation notes:
 * - Implements all methods of the ShaderCache class
 * - Includes ShaderSource management, ShaderProgram management, ProgramId mapping management and statistics
 * - Adds robust error handling (LogError + ERROR_AND_DIE) and try-catch exception handling
 * - Uses #pragma region to organize code
 */

#include "ShaderCache.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Core/LogCategory/PredefinedCategories.hpp"
#include "Engine/Graphic/Shader/ShaderPack/ShaderSource.hpp"
#include "Engine/Graphic/Shader/ShaderPack/ShaderProgram.hpp"
using namespace enigma::core;

namespace enigma::graphic
{
#pragma region ShaderSource Management

    void ShaderCache::CacheSource(const std::string& name, std::shared_ptr<ShaderSource> source)
    {
        // Parameter validation
        if (name.empty())
        {
            LogError(LogRenderer, "ShaderCache::CacheSource: name is empty");
            ERROR_AND_DIE("ShaderCache::CacheSource: name cannot be empty");
        }

        if (!source)
        {
            LogError(LogRenderer, "ShaderCache::CacheSource: source is nullptr for name '%s'", name.c_str());
            ERROR_AND_DIE("ShaderCache::CacheSource: source cannot be nullptr");
        }

        try
        {
            m_sources[name] = std::move(source);
            LogInfo(LogRenderer, "ShaderCache::CacheSource: cached source '%s'", name.c_str());
        }
        catch (const std::exception& e)
        {
            LogError(LogRenderer, "ShaderCache::CacheSource: exception for name '%s': %s", name.c_str(), e.what());
            ERROR_AND_DIE("ShaderCache::CacheSource: failed to cache source");
        }
    }

    std::shared_ptr<ShaderSource> ShaderCache::GetSource(const std::string& name) const
    {
        auto it = m_sources.find(name);
        if (it == m_sources.end())
        {
            LogError(LogRenderer, "ShaderCache::GetSource: source not found for name '%s'", name.c_str());
            return nullptr;
        }
        return it->second;
    }

    std::shared_ptr<ShaderSource> ShaderCache::GetSource(ProgramId id) const
    {
        std::string name = GetProgramName(id);
        if (name.empty())
        {
            LogError(LogRenderer, "ShaderCache::GetSource: ProgramId %d not registered", static_cast<int>(id));
            return nullptr;
        }
        return GetSource(name);
    }

    bool ShaderCache::HasSource(const std::string& name) const
    {
        return m_sources.find(name) != m_sources.end();
    }

    bool ShaderCache::RemoveSource(const std::string& name)
    {
        auto sourceIt = m_sources.find(name);
        if (sourceIt == m_sources.end())
        {
            LogWarn(LogRenderer, "ShaderCache::RemoveSource: source not found for name '%s'", name.c_str());
            return false;
        }

        // 同步删除ShaderProgram
        m_programs.erase(name);
        m_sources.erase(sourceIt);

        LogInfo(LogRenderer, "ShaderCache::RemoveSource: removed source and program '%s'", name.c_str());
        return true;
    }

    void ShaderCache::ClearSources()
    {
        size_t sourceCount  = m_sources.size();
        size_t programCount = m_programs.size();

        m_sources.clear();
        m_programs.clear(); // 同步清空ShaderProgram

        LogInfo(LogRenderer, "ShaderCache::ClearSources: cleared %zu sources and %zu programs", sourceCount, programCount);
    }

#pragma endregion

#pragma region ShaderProgram Management

    void ShaderCache::CacheProgram(const std::string& name, std::shared_ptr<ShaderProgram> program)
    {
        // Parameter validation
        if (name.empty())
        {
            LogError(LogRenderer, "ShaderCache::CacheProgram: name is empty");
            ERROR_AND_DIE("ShaderCache::CacheProgram: name cannot be empty");
        }

        if (!program)
        {
            LogError(LogRenderer, "ShaderCache::CacheProgram: program is nullptr for name '%s'", name.c_str());
            ERROR_AND_DIE("ShaderCache::CacheProgram: program cannot be nullptr");
        }

        try
        {
            m_programs[name] = std::move(program);
            LogInfo(LogRenderer, "ShaderCache::CacheProgram: cached program '%s'", name.c_str());
        }
        catch (const std::exception& e)
        {
            LogError(LogRenderer, "ShaderCache::CacheProgram: exception for name '%s': %s", name.c_str(), e.what());
            ERROR_AND_DIE("ShaderCache::CacheProgram: failed to cache program");
        }
    }

    std::shared_ptr<ShaderProgram> ShaderCache::GetProgram(const std::string& name) const
    {
        auto it = m_programs.find(name);
        if (it == m_programs.end())
        {
            LogError(LogRenderer, "ShaderCache::GetProgram: program not found for name '%s'", name.c_str());
            return nullptr;
        }
        return it->second;
    }

    std::shared_ptr<ShaderProgram> ShaderCache::GetProgram(ProgramId id) const
    {
        std::string name = GetProgramName(id);
        if (name.empty())
        {
            LogError(LogRenderer, "ShaderCache::GetProgram: ProgramId %d not registered", static_cast<int>(id));
            return nullptr;
        }
        return GetProgram(name);
    }

    bool ShaderCache::HasProgram(const std::string& name) const
    {
        return m_programs.find(name) != m_programs.end();
    }

    bool ShaderCache::RemoveProgram(const std::string& name)
    {
        auto it = m_programs.find(name);
        if (it == m_programs.end())
        {
            LogWarn(LogRenderer, "ShaderCache::RemoveProgram: program not found for name '%s'", name.c_str());
            return false;
        }

        m_programs.erase(it);
        LogInfo(LogRenderer, "ShaderCache::RemoveProgram: removed program '%s'", name.c_str());
        return true;
    }

    void ShaderCache::ClearPrograms()
    {
        size_t programCount = m_programs.size();
        m_programs.clear();
        LogInfo(LogRenderer, "ShaderCache::ClearPrograms: cleared %zu programs (sources retained for hot reload)", programCount);
    }

#pragma endregion

#pragma region ProgramId Mapping Management

    void ShaderCache::RegisterProgramId(ProgramId id, const std::string& name)
    {
        // Parameter validation
        if (name.empty())
        {
            LogError(LogRenderer, "ShaderCache::RegisterProgramId: name is empty for ProgramId %d", static_cast<int>(id));
            ERROR_AND_DIE("ShaderCache::RegisterProgramId: name cannot be empty");
        }

        // Check for duplicate registration
        auto it = m_idToName.find(id);
        if (it != m_idToName.end())
        {
            LogError(LogRenderer, "ShaderCache::RegisterProgramId: ProgramId %d already registered with name '%s'",
                     static_cast<int>(id), it->second.c_str());
            ERROR_AND_DIE("ShaderCache::RegisterProgramId: duplicate registration");
        }

        try
        {
            m_idToName[id] = name;
            LogInfo(LogRenderer, "ShaderCache::RegisterProgramId: registered ProgramId %d -> '%s'", static_cast<int>(id), name.c_str());
        }
        catch (const std::exception& e)
        {
            LogError(LogRenderer, "ShaderCache::RegisterProgramId: exception for ProgramId %d: %s", static_cast<int>(id), e.what());
            ERROR_AND_DIE("ShaderCache::RegisterProgramId: failed to register mapping");
        }
    }

    std::string ShaderCache::GetProgramName(ProgramId id) const
    {
        auto it = m_idToName.find(id);
        if (it == m_idToName.end())
        {
            // Return empty string to indicate not registered (no error logging, as this is a normal query operation)
            return "";
        }
        return it->second;
    }

    void ShaderCache::RegisterProgramIds(const std::unordered_map<ProgramId, std::string>& mappings)
    {
        try
        {
            for (const auto& [id, name] : mappings)
            {
                RegisterProgramId(id, name);
            }
            LogInfo(LogRenderer, "ShaderCache::RegisterProgramIds: registered %zu mappings", mappings.size());
        }
        catch (const std::exception& e)
        {
            LogError(LogRenderer, "ShaderCache::RegisterProgramIds: exception during batch registration: %s", e.what());
            ERROR_AND_DIE("ShaderCache::RegisterProgramIds: failed to register mappings");
        }
    }

#pragma endregion

#pragma region Statistics

    size_t ShaderCache::GetSourceCount() const
    {
        return m_sources.size();
    }

    size_t ShaderCache::GetProgramCount() const
    {
        return m_programs.size();
    }

    size_t ShaderCache::GetProgramIdCount() const
    {
        return m_idToName.size();
    }

#pragma endregion
} // namespace enigma::graphic
