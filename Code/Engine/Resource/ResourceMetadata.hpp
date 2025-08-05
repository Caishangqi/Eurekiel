#pragma once
#include "ResourceCommon.hpp"
#include <filesystem>
#include <chrono>
#include <any>
#include <vector>

namespace enigma::resource
{
    /**
     * @brief Represents metadata information of a resource.
     *
     * This struct contains essential metadata about a resource, including its location, file path, type, and state.
     * It also includes additional data such as file information (size and last modification time),
     * dependencies (reserved for future use), and extended metadata (reserved for future use).
     */
    struct ResourceMetadata
    {
        ResourceLocation      location;
        std::filesystem::path filePath;
        ResourceType          type;
        ResourceState         state = ResourceState::NOT_LOADED;

        // File Information
        std::uintmax_t                  fileSize = 0;
        std::filesystem::file_time_type lastModified;

        // Dependency information (reserved for the future)
        std::vector<ResourceLocation> dependencies;

        // Extended metadata (reserved for the future)
        std::any extendedData;

        // Constructor
        ResourceMetadata() = default;
        ResourceMetadata(const ResourceLocation& loc, const std::filesystem::path& path);

        // Utility function
        static ResourceType DetectType(const std::filesystem::path& path);
        std::string         GetFileExtension() const;
    };

    /**
     * @brief Interface for a resource abstraction.
     *
     * This class defines a common interface for handling resources, providing methods
     * to retrieve metadata, resource type, and load status. It also facilitates
     * access to raw binary data associated with the resource, if available.
     *
     * Implementations of this interface represent specific types of resources, such as
     * textures, models, shaders, audio files, or binary data containers.
     */
    class IResource
    {
    public:
        virtual ~IResource() = default;

        // Get resource metadata
        virtual const ResourceMetadata& GetMetadata() const = 0;

        // Get the resource type
        virtual ResourceType GetType() const = 0;

        // Whether it is loaded
        virtual bool IsLoaded() const = 0;

        // Get raw data (if available)
        virtual const void* GetRawData() const { return nullptr; }
        virtual size_t      GetRawDataSize() const { return 0; }
    };

    /**
     * @brief Represents a raw resource containing unprocessed binary data.
     *
     * This class provides functionality for managing raw binary data alongside
     * its associated metadata. It inherits from the IResource interface and
     * implements methods to retrieve metadata, resource type, load status, and
     * the raw binary data itself.
     *
     * Instances of RawResource are initialized with metadata and binary data,
     * transitioning the resource state to LOADED upon construction.
     */
    class RawResource : public IResource
    {
    public:
        RawResource(const ResourceMetadata& metadata, std::vector<uint8_t> data) : m_metadata(metadata), m_data(std::move(data))
        {
            m_metadata.state = ResourceState::LOADED;
        }

        const ResourceMetadata& GetMetadata() const override { return m_metadata; }
        ResourceType            GetType() const override { return m_metadata.type; }
        bool                    IsLoaded() const override { return !m_data.empty(); }

        const void* GetRawData() const override { return m_data.data(); }
        size_t      GetRawDataSize() const override { return m_data.size(); }

        // Get data references
        const std::vector<uint8_t>& GetData() const { return m_data; }

    private:
        ResourceMetadata     m_metadata;
        std::vector<uint8_t> m_data;
    };
}
