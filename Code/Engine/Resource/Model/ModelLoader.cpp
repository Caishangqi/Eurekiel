#include "ModelLoader.hpp"
#include "../../Core/Json.hpp"
#include <iostream>
#include <sstream>

using namespace enigma::resource::model;
using namespace enigma::resource;
using namespace enigma::core;

ModelLoader::ModelLoader()
{
    // Initialize supported extensions for JSON model files
    m_supportedExtensions = {
        ".json" // Standard JSON model files
    };
}

ResourcePtr ModelLoader::Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data)
{
    if (!CanLoad(metadata))
    {
        return nullptr;
    }

    try
    {
        return LoadModelFromJson(data, metadata.location);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[ModelLoader] Failed to load model " << metadata.location.ToString()
            << ": " << e.what() << std::endl;
        return nullptr;
    }
}

std::set<std::string> ModelLoader::GetSupportedExtensions() const
{
    return m_supportedExtensions;
}

std::string ModelLoader::GetLoaderName() const
{
    return "ModelLoader";
}

int ModelLoader::GetPriority() const
{
    return 200; // Higher priority than RawResourceLoader (which is typically 50-100)
}

bool ModelLoader::CanLoad(const ResourceMetadata& metadata) const
{
    // Check if it's a JSON file in a models/ directory
    if (!IsModelFormat(metadata.GetFileExtension()))
        return false;

    // Check if the path contains "models/" 
    const std::string& path = metadata.location.GetPath();
    return path.find("models/") != std::string::npos;
}

bool ModelLoader::IsModelFormat(const std::string& extension) const
{
    return m_supportedExtensions.find(extension) != m_supportedExtensions.end();
}

std::shared_ptr<ModelResource> ModelLoader::LoadModelFromJson(const std::vector<uint8_t>& data, const ResourceLocation& location) const
{
    // Convert binary data to string
    std::string jsonString(data.begin(), data.end());

    // Parse JSON using JsonObject
    auto jsonOpt = JsonObject::TryParse(jsonString);
    if (!jsonOpt.has_value())
    {
        throw std::runtime_error("Failed to parse JSON");
    }

    // Create ModelResource
    auto model = std::make_shared<ModelResource>(location);

    // Load from JSON
    if (!model->LoadFromJson(jsonOpt.value()))
    {
        throw std::runtime_error("Failed to load model from JSON data");
    }

    return model;
}
