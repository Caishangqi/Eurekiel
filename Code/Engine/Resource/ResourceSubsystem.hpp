#pragma once
#include "ResourceCommon.hpp"
#include "Provider/ResourceProvider.hpp"
#include "Loader/ResourceLoader.hpp"
#include "../Core/SubsystemManager.hpp"
#include <mutex>
#include <shared_mutex>
#include <future>
#include <queue>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <chrono>


namespace enigma::resource
{
    /**
     * @struct ResourceConfig
     * @brief Resource subsystem configuration structure
     * 
     * Contains all configuration parameters for the resource management system
     */
    struct ResourceConfig
    {
        // Basic resource configuration
        std::filesystem::path baseAssetPath      = ".enigma/assets";
        size_t                maxCacheSize       = (size_t)512 * 1024 * 1024; // 512MB default
        size_t                maxCachedResources = 1000;
        size_t                loadThreadCount    = 2;

        // Hot reload configuration
        bool  enableHotReload        = false;
        float hotReloadCheckInterval = 1.0f; // seconds

        // Memory mapping configuration
        bool   useMemoryMapping        = false;
        size_t minFileSizeForMemoryMap = (size_t)1024 * 1024; // 1MB

        // Async loading configuration
        size_t minFileSizeForAsync   = (size_t)100 * 1024; // 100KB
        bool   enableParallelLoading = true;
        size_t asyncLoadQueueSize    = 100;

        // Cache configuration
        bool  enableLRUCache         = true;
        bool  compressCache          = false;
        float cacheEvictionThreshold = 0.9f; // Start eviction when 90% full

        // Debug configuration
        bool logResourceLoads          = false;
        bool logCacheMisses            = false;
        bool logCacheEvictions         = false;
        bool validateResourcePaths     = true;
        bool printScanResults          = false;
        bool traceResourceDependencies = false;

        // Performance limits (disabled - load resources immediately like Minecraft Neoforge)
        // struct PerformanceLimits {} performanceLimits;

        // Namespace configuration
        struct NamespaceEntry
        {
            std::string              name;
            std::filesystem::path    customPath;
            bool                     autoScan   = true;
            bool                     preloadAll = false;
            std::vector<std::string> preloadPatterns; // e.g. "textures/ui/*"
        };

        std::vector<NamespaceEntry> namespaces = {
            {"engine", "", true, false, {}}
        };

        // File extension mappings
        struct ExtensionMapping
        {
            std::string  extension;
            ResourceType type;
        };

        std::vector<ExtensionMapping> extensionMappings = {
            {".png", ResourceType::TEXTURE},
            {".jpg", ResourceType::TEXTURE},
            {".jpeg", ResourceType::TEXTURE},
            {".bmp", ResourceType::TEXTURE},
            {".tga", ResourceType::TEXTURE},
            {".dds", ResourceType::TEXTURE},
            {".obj", ResourceType::MODEL},
            {".fbx", ResourceType::MODEL},
            {".gltf", ResourceType::MODEL},
            {".glb", ResourceType::MODEL},
            {".dae", ResourceType::MODEL},
            {".wav", ResourceType::SOUND},
            {".wave", ResourceType::SOUND},
            {".mp3", ResourceType::SOUND},
            {".ogg", ResourceType::SOUND},
            {".flac", ResourceType::SOUND},
            {".m4a", ResourceType::SOUND},
            {".mp4", ResourceType::SOUND},
            {".aiff", ResourceType::SOUND},
            {".aif", ResourceType::SOUND},
            {".wma", ResourceType::SOUND},
            {".vert", ResourceType::SHADER},
            {".frag", ResourceType::SHADER},
            {".glsl", ResourceType::SHADER},
            {".hlsl", ResourceType::SHADER},
            {".shader", ResourceType::SHADER}
        };

        bool         IsValid();
        void         AddNamespace(const std::string& name, const std::filesystem::path& path = "");
        void         EnableNamespacePreload(const std::string& name, const std::vector<std::string>& patterns = {});
        ResourceType GetTypeForExtension(const std::string& ext) const;
    };
}

namespace enigma::resource
{
    /**
 * @class ResourceSubsystem
 * @brief Manages the discovery, loading, caching, and management of file system resources.
 *
 * The ResourceSubsystem is responsible for interacting with the file system to locate resources,
 * load them into memory, cache them for efficient access, and manage their lifecycles.
 *
 * This class provides an abstraction over the raw file system operations, enabling applications
 * to focus on high-level resource management without dealing with low-level details.
 */
    class ResourceSubsystem : public enigma::core::EngineSubsystem
    {
    public:
        explicit ResourceSubsystem(ResourceConfig& config);
        ~ResourceSubsystem() override;

        // Disable copy and move
        ResourceSubsystem(const ResourceSubsystem&)            = delete;
        ResourceSubsystem& operator=(const ResourceSubsystem&) = delete;
        ResourceSubsystem(ResourceSubsystem&&)                 = delete;
        ResourceSubsystem& operator=(ResourceSubsystem&&)      = delete;

        // EngineSubsystem interface
        DECLARE_SUBSYSTEM(ResourceSubsystem, "resource", 100);
        void Startup() override;
        void Shutdown() override;
        bool RequiresGameLoop() const override { return false; }

        /// Update for resource management (hot reload, async loading, etc.)
        void Update(); // Called manually when needed, not part of game loop

        /// Configuration
        ResourceConfig&       GetConfig() { return m_config; }
        const ResourceConfig& GetConfig() const { return m_config; }

        /// Provider Management
        void                                            AddResourceProvider(std::shared_ptr<IResourceProvider> provider);
        void                                            RemoveResourceProvider(const std::string& name);
        std::vector<std::shared_ptr<IResourceProvider>> GetResourceProviders() const;

        /// Loader Management
        LoaderRegistry&       GetLoaderRegistry() { return m_loaderRegistry; }
        const LoaderRegistry& GetLoaderRegistry() const { return m_loaderRegistry; }

        void RegisterLoader(std::shared_ptr<IResourceLoader> loader)
        {
            m_loaderRegistry.RegisterLoader(loader);
        }

        /// Resource Access
        bool                     HasResource(const ResourceLocation& location) const;
        ResourcePtr              GetResource(const ResourceLocation& location);
        std::future<ResourcePtr> GetResourceAsync(const ResourceLocation& location);
        void                     PreloadAllResources(std::function<void(size_t loaded, size_t total)> callback = nullptr);

        /// Resource Queries
        std::optional<ResourceMetadata> GetMetadata(const ResourceLocation& location) const;
        std::vector<ResourceLocation>   ListResources(const std::string& namespaceName = "", ResourceType type = ResourceType::UNKNOWN) const;
        std::vector<ResourceLocation>   SearchResources(const std::string& pattern, ResourceType type = ResourceType::UNKNOWN) const;

        /// Resource Management
        void ClearAllResources();
        void UnloadResource(const ResourceLocation& location);

        struct ResourceStats
        {
            size_t totalSize     = 0;
            size_t resourceCount = 0;
            size_t totalLoaded   = 0;
        };

        ResourceStats GetResourceStats() const;

        /// Resource Scanning
        void ScanResources(std::function<void(const std::string& current, size_t scanned)> callback = nullptr);
        void ScanNamespace(const std::string& namespaceName);

        /// Hot Reload
        size_t CheckAndReloadModifiedResources();

        /// Performance Stats
        struct PerformanceStats
        {
            size_t loadAttemptsThisFrame = 0;
            size_t bytesLoadedThisFrame  = 0;
            double loadTimeThisFrame     = 0.0;
            bool   isLoadLimited         = false;
            size_t asyncQueueSize        = 0;
            size_t activeLoadThreads     = 0;
        };

        PerformanceStats GetPerformanceStats() const { return m_perfStats; }

    private:
        /// Internal Methods
        void                               InitializeDefaultLoaders();
        void                               InitializeDefaultProviders();
        void                               StartWorkerThreads();
        void                               StopWorkerThreads();
        void                               WorkerThreadFunc();
        ResourcePtr                        LoadResourceInternal(const ResourceLocation& location);
        void                               UpdateResourceIndex();
        std::shared_ptr<IResourceProvider> FindProviderForResource(const ResourceLocation& location) const;
        bool                               MatchesPattern(const std::string& str, const std::string& pattern) const;
        void                               ProcessNamespacePreloads();
        void                               UpdateFrameStatistics();
        bool                               ShouldStopLoadingThisFrame() const;
        void                               PreloadAllDiscoveredResources();

    private:
        // Configuration (reference, not owned)
        ResourceConfig& m_config;

        // Subsystem state
        enum class SubsystemState
        {
            UNINITIALIZED,
            READY,
            SHUTTING_DOWN
        };

        SubsystemState m_state = SubsystemState::UNINITIALIZED;
        LoaderRegistry m_loaderRegistry; // Loader registry

        // Resource index (all discovered resources)
        mutable std::shared_mutex                              m_indexMutex;
        std::unordered_map<ResourceLocation, ResourceMetadata> m_resourceIndex;

        // Preloaded resources storage
        mutable std::shared_mutex                      m_resourceMutex;
        std::unordered_map<ResourceLocation, ResourcePtr> m_loadedResources;

        // Resource statistics
        mutable std::atomic<size_t> m_totalLoaded{0};

        // Resource providers
        mutable std::shared_mutex                       m_providerMutex;
        std::vector<std::shared_ptr<IResourceProvider>> m_resourceProviders;

        // Async loading queue
        struct LoadRequest
        {
            ResourceLocation                      location;
            std::promise<ResourcePtr>             promise;
            std::chrono::steady_clock::time_point requestTime;
            size_t                                estimatedSize = 0;
        };

        std::queue<LoadRequest> m_loadQueue;
        std::mutex              m_queueMutex;
        std::condition_variable m_queueCV;

        // Worker threads
        std::vector<std::thread> m_workerThreads;
        std::atomic<bool>        m_running{false};

        // Hot reload tracking
        std::chrono::steady_clock::time_point                                 m_lastHotReloadCheck;
        std::unordered_map<ResourceLocation, std::filesystem::file_time_type> m_fileModificationTimes;

        // Performance tracking
        std::chrono::steady_clock::time_point m_frameStartTime;
        PerformanceStats                      m_perfStats;
    };
}
