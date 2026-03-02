// ============================================================================
// MaterialIdMapper.cpp - Block name to material ID mapping implementation
// ============================================================================

#include "MaterialIdMapper.hpp"
#include "Engine/Core/Properties.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"

#include <sstream>

namespace enigma::graphic
{
    bool MaterialIdMapper::Load(const std::filesystem::path& blockPropertiesPath)
    {
        m_blockToMaterialId.clear();

        // Create PropertiesFile transiently - do NOT hold as member
        // (avoids retaining m_originalContent string memory)
        enigma::core::PropertiesFile props;
        if (!props.Load(blockPropertiesPath))
        {
            LogWarn(LogShaderBundle, "MaterialIdMapper: Failed to load block.properties from '%s'",
                    blockPropertiesPath.string().c_str());
            return false;
        }

        // Parse "block.NNNNN=namespace:name1 namespace:name2 ..." entries
        const auto& allProperties = props.GetAll();
        for (const auto& [key, value] : allProperties)
        {
            // Only process keys starting with "block."
            if (key.rfind("block.", 0) != 0)
                continue;

            // Extract material ID from key: "block.32000" -> 32000
            const std::string idStr      = key.substr(6); // skip "block."
            int               materialId = 0;
            try
            {
                materialId = std::stoi(idStr);
            }
            catch (const std::exception&)
            {
                LogWarn(LogShaderBundle, "MaterialIdMapper: Invalid material ID in key '%s'",
                        key.c_str());
                continue;
            }

            if (materialId <= 0 || materialId > 65535)
            {
                LogWarn(LogShaderBundle, "MaterialIdMapper: Material ID %d out of uint16_t range in key '%s'",
                        materialId, key.c_str());
                continue;
            }

            // Parse space-separated block names from value
            // e.g., "simpleminer:water" or "simpleminer:ice simpleminer:packed_ice"
            std::istringstream stream(value);
            std::string        blockName;
            while (stream >> blockName)
            {
                if (blockName.empty())
                    continue;

                m_blockToMaterialId[blockName] = static_cast<uint16_t>(materialId);
            }
        }

        // PropertiesFile (props) destroyed here - no lingering m_originalContent memory

        LogInfo(LogShaderBundle, "MaterialIdMapper: Loaded %zu block-to-material mappings from '%s'",
                m_blockToMaterialId.size(), blockPropertiesPath.string().c_str());

        return !m_blockToMaterialId.empty();
    }

    uint16_t MaterialIdMapper::GetMaterialId(const std::string& namespacedName) const
    {
        auto it = m_blockToMaterialId.find(namespacedName);
        if (it != m_blockToMaterialId.end())
            return it->second;
        return 0;
    }

    void MaterialIdMapper::OnBuildVertex(TerrainVertex* vertices, const std::string& blockName)
    {
        uint16_t materialId = GetMaterialId(blockName);
        if (materialId == 0)
            return; // No special material, skip

        for (int i = 0; i < 4; ++i)
            vertices[i].m_entityId = materialId;
    }
} // namespace enigma::graphic
