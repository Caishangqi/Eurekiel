#include "BlockStateLoader.hpp"
#include "../../Core/Json.hpp"
#include <iostream>
#include <sstream>

using namespace enigma::resource::blockstate;
using namespace enigma::resource;
using namespace enigma::core;

BlockStateLoader::BlockStateLoader()
{
    // Initialize supported extensions for JSON blockstate files
    m_supportedExtensions = {
        ".json" // Standard JSON blockstate files
    };
}

ResourcePtr BlockStateLoader::Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data)
{
    if (!CanLoad(metadata))
    {
        return nullptr;
    }

    try
    {
        return LoadBlockStateFromJson(data, metadata.location);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[BlockStateLoader] Failed to load blockstate " << metadata.location.ToString()
            << ": " << e.what() << '\n';
        return nullptr;
    }
}

std::set<std::string> BlockStateLoader::GetSupportedExtensions() const
{
    return m_supportedExtensions;
}

std::string BlockStateLoader::GetLoaderName() const
{
    return "BlockStateLoader";
}

int BlockStateLoader::GetPriority() const
{
    return 200; // Higher priority than RawResourceLoader (which is typically 50-100)
}

bool BlockStateLoader::CanLoad(const ResourceMetadata& metadata) const
{
    // Check if it's a JSON file in a blockstates/ directory
    if (!IsBlockStateFormat(metadata.GetFileExtension()))
        return false;

    // Check if the path contains "blockstates/" 
    const std::string& path = metadata.location.GetPath();
    return path.find("blockstates/") != std::string::npos;
}

bool BlockStateLoader::IsBlockStateFormat(const std::string& extension) const
{
    return m_supportedExtensions.find(extension) != m_supportedExtensions.end();
}

std::shared_ptr<BlockStateDefinition> BlockStateLoader::LoadBlockStateFromJson(const std::vector<uint8_t>& data, const ResourceLocation& location) const
{
    // Convert binary data to string
    std::string jsonString(data.begin(), data.end());

    // Parse JSON using the custom Json library
    auto jsonOpt = JsonObject::TryParse(jsonString);
    if (!jsonOpt.has_value())
    {
        throw std::runtime_error("Failed to parse JSON");
    }

    // Create BlockStateDefinition
    auto blockState = std::make_shared<BlockStateDefinition>(location);

    // Load from JSON
    if (!blockState->LoadFromJson(jsonOpt.value()))
    {
        throw std::runtime_error("Failed to load blockstate from JSON data");
    }

    return blockState;
}
