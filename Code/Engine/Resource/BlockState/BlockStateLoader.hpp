#pragma once
#include "../ResourceLoader.hpp"
#include "../ResourceCommon.hpp"
#include "BlockStateDefinition.hpp"
#include <set>
#include <string>
#include <memory>

namespace enigma::resource::blockstate
{
    using namespace enigma::resource;

    /**
     * BlockStateLoader - Resource loader for BlockStateDefinition objects
     * Supports JSON blockstate files following Minecraft blockstate format
     * Implements IResourceLoader interface for integration with ResourceSubsystem
     */
    class BlockStateLoader : public IResourceLoader
    {
    public:
        BlockStateLoader();
        virtual ~BlockStateLoader() override = default;

        // IResourceLoader interface implementation
        ResourcePtr           Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data) override;
        std::set<std::string> GetSupportedExtensions() const override;
        std::string           GetLoaderName() const override;
        int                   GetPriority() const override;

        // Additional validation
        bool CanLoad(const ResourceMetadata& metadata) const override;

    private:
        // Supported blockstate formats
        std::set<std::string> m_supportedExtensions;

        // Helper methods
        bool                                  IsBlockStateFormat(const std::string& extension) const;
        std::shared_ptr<BlockStateDefinition> LoadBlockStateFromJson(const std::vector<uint8_t>& data, const ResourceLocation& location) const;
    };
}
