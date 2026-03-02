#pragma once

// ============================================================================
// MaterialIdMapper.hpp - Block name to material ID mapping
//
// Loads block.properties (Iris-compatible format) and maps
// namespace:registryName -> uint16_t materialId.
// Used by ShaderBundle to inject material IDs into terrain vertices
// via TerrainVertexLayout::OnBuildVertexLayout event.
//
// Design:
//   - PropertiesFile is created transiently in Load(), NOT held as member
//     (avoids m_originalContent memory overhead)
//   - GetMaterialId() returns 0 for unknown blocks (no special material)
//   - OnBuildVertex() is the event callback, sets m_entityId on 4 vertices
//
// Format (block.properties):
//   block.32000=simpleminer:water
//   block.32004=simpleminer:ice simpleminer:packed_ice simpleminer:blue_ice
// ============================================================================

#include <cstdint>
#include <string>
#include <unordered_map>
#include <filesystem>

namespace enigma::graphic
{
    struct TerrainVertex;

    class MaterialIdMapper
    {
    public:
        MaterialIdMapper()  = default;
        ~MaterialIdMapper() = default;

        // Non-copyable, movable
        MaterialIdMapper(const MaterialIdMapper&)                = delete;
        MaterialIdMapper& operator=(const MaterialIdMapper&)     = delete;
        MaterialIdMapper(MaterialIdMapper&&) noexcept            = default;
        MaterialIdMapper& operator=(MaterialIdMapper&&) noexcept = default;

        // Load block.properties, parse "block.NNNNN=ns:name1 ns:name2" entries.
        // PropertiesFile is created and destroyed within this call.
        // Returns true if file loaded and at least one mapping was parsed.
        bool Load(const std::filesystem::path& blockPropertiesPath);

        // Query namespace:registryName -> materialId.
        // Returns 0 if the block has no special material mapping.
        uint16_t GetMaterialId(const std::string& namespacedName) const;

        // Event callback for TerrainVertexLayout::OnBuildVertexLayout.
        // Sets m_entityId on all 4 quad vertices if blockName has a mapping.
        void OnBuildVertex(TerrainVertex* vertices, const std::string& blockName);

        // Returns number of loaded mappings (for diagnostics)
        size_t GetMappingCount() const { return m_blockToMaterialId.size(); }

    private:
        // "simpleminer:water" -> 32000, "simpleminer:ice" -> 32004
        std::unordered_map<std::string, uint16_t> m_blockToMaterialId;
    };
} // namespace enigma::graphic
