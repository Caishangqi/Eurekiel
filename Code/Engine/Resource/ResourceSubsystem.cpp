﻿#include "ResourceSubsystem.hpp"

#include "ResourceSubsystem.hpp"
#include "Provider/ResourceProvider.hpp"
#include "Loader/ResourceLoader.hpp"
#include <iostream>
#include <algorithm>
#include <regex>


using namespace enigma::resource;

bool ResourceConfig::IsValid()
{
    return !baseAssetPath.empty() &&
        maxCacheSize > 0 &&
        maxCachedResources > 0 &&
        loadThreadCount > 0 &&
        hotReloadCheckInterval > 0;
}

void ResourceConfig::AddNamespace(const std::string& name, const std::filesystem::path& path)
{
    namespaces.push_back({name, path, true, false, {}});
}

void ResourceConfig::EnableNamespacePreload(const std::string& name, const std::vector<std::string>& patterns)
{
    for (auto& ns : namespaces)
    {
        if (ns.name == name)
        {
            ns.preloadAll      = patterns.empty();
            ns.preloadPatterns = patterns;
            break;
        }
    }
}

ResourceType ResourceConfig::GetTypeForExtension(const std::string& ext) const
{
    for (const auto& mapping : extensionMappings)
    {
        if (mapping.extension == ext)
        {
            return mapping.type;
        }
    }
    return ResourceType::UNKNOWN;
}

ResourceSubsystem::ResourceSubsystem(ResourceConfig& config) : m_config(config)
{
    if (!m_config.IsValid())
    {
        std::cerr << "[ResourceSubsystem] Invalid ResourceConfig provided! Using defaults." << std::endl;
        m_config = ResourceConfig{}; // Use default configuration
    }
}

ResourceSubsystem::~ResourceSubsystem()
{
    if (m_state != SubsystemState::UNINITIALIZED)
    {
        Shutdown();
    }
}

void ResourceSubsystem::Startup()
{
    if (m_state != SubsystemState::UNINITIALIZED)
    {
        std::cerr << "[ResourceSubsystem] Already initialized!" << '\n';
        return;
    }

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Starting up..." << '\n';
    }

    // Initialize default loaders
    InitializeDefaultLoaders();

    // Initialize default providers
    InitializeDefaultProviders();

    // Start worker threads for async loading
    StartWorkerThreads();

    // Initial resource scan
    ScanResources();

    // Preload all discovered resources
    PreloadAllDiscoveredResources();

    m_state = SubsystemState::READY;

    if (m_config.printScanResults)
    {
        std::cout << "[ResourceSubsystem] Startup complete. Found "
            << m_resourceIndex.size() << " resources." << '\n';
    }
}

void ResourceSubsystem::Shutdown()
{
    if (m_state == SubsystemState::UNINITIALIZED)
    {
        return;
    }

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Shutting down..." << '\n';
    }

    m_state = SubsystemState::SHUTTING_DOWN;

    // Stop worker threads
    StopWorkerThreads();

    // Clear all resources
    ClearAllResources();

    // Clear providers
    {
        std::unique_lock<std::shared_mutex> lock(m_providerMutex);
        m_resourceProviders.clear();
    }

    // Clear loaders
    m_loaderRegistry.Clear();

    // Clear index
    {
        std::unique_lock<std::shared_mutex> lock(m_indexMutex);
        m_resourceIndex.clear();
    }

    m_state = SubsystemState::UNINITIALIZED;

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Shutdown complete." << '\n';
    }
}

void ResourceSubsystem::Update()
{
    if (m_state != SubsystemState::READY)
        return;
    // TODO: Handle statistic and performance updates, and hot reload
}

void ResourceSubsystem::AddResourceProvider(std::shared_ptr<IResourceProvider> provider)
{
    if (!provider) return;

    std::unique_lock<std::shared_mutex> lock(m_providerMutex);
    m_resourceProviders.push_back(provider);

    // Sort by priority (higher priority first)
    std::sort(m_resourceProviders.begin(), m_resourceProviders.end(),
              [](const auto& a, const auto& b)
              {
                  return a->GetPriority() > b->GetPriority();
              });

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Added provider: " << provider->GetName() << std::endl;
    }
}

void ResourceSubsystem::RemoveResourceProvider(const std::string& name)
{
    std::unique_lock<std::shared_mutex> lock(m_providerMutex);
    m_resourceProviders.erase(
        std::remove_if(m_resourceProviders.begin(), m_resourceProviders.end(),
                       [&name](const auto& provider)
                       {
                           return provider->GetName() == name;
                       }),
        m_resourceProviders.end()
    );
}

std::vector<std::shared_ptr<IResourceProvider>> ResourceSubsystem::GetResourceProviders() const
{
    std::shared_lock<std::shared_mutex> lock(m_providerMutex);
    return m_resourceProviders;
}

bool ResourceSubsystem::HasResource(const ResourceLocation& location) const
{
    std::shared_lock<std::shared_mutex> lock(m_indexMutex);
    return m_resourceIndex.find(location) != m_resourceIndex.end();
}

ResourcePtr ResourceSubsystem::GetResource(const ResourceLocation& location)
{
    // Check exact match first
    {
        std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
        auto                                it = m_preloadedResources.find(location);
        if (it != m_preloadedResources.end())
        {
            return it->second;
        }
    }

    // Since ResourceLocation no longer contains extensions, exact match failure means resource not found

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Resource not preloaded: " << location.ToString() << std::endl;
    }

    // Resource not found in preloaded resources
    return nullptr;
}

std::future<ResourcePtr> ResourceSubsystem::GetResourceAsync(const ResourceLocation& location)
{
    // Since all resources are preloaded, return immediately
    std::promise<ResourcePtr> promise;
    auto resource = GetResource(location);
    promise.set_value(resource);
    return promise.get_future();
}

void ResourceSubsystem::PreloadAllResources(std::function<void(size_t, size_t)> callback)
{
    PreloadAllDiscoveredResources();
    
    if (callback)
    {
        std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
        callback(m_preloadedResources.size(), m_preloadedResources.size());
    }
}

std::optional<ResourceMetadata> ResourceSubsystem::GetMetadata(const ResourceLocation& location) const
{
    std::shared_lock<std::shared_mutex> lock(m_indexMutex);
    auto                                it = m_resourceIndex.find(location);
    if (it != m_resourceIndex.end())
    {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ResourceLocation> ResourceSubsystem::ListResources(const std::string& namespaceName, ResourceType type) const
{
    std::vector<ResourceLocation>       results;
    std::shared_lock<std::shared_mutex> lock(m_indexMutex);

    for (const auto& [location, metadata] : m_resourceIndex)
    {
        bool matchNamespace = namespaceName.empty() || location.GetNamespace() == namespaceName;
        bool matchType      = type == ResourceType::UNKNOWN || metadata.type == type;

        if (matchNamespace && matchType)
        {
            results.push_back(location);
        }
    }

    return results;
}

std::vector<ResourceLocation> ResourceSubsystem::SearchResources(const std::string& pattern, ResourceType type) const
{
    std::vector<ResourceLocation>       results;
    std::shared_lock<std::shared_mutex> lock(m_indexMutex);

    for (const auto& [location, metadata] : m_resourceIndex)
    {
        if (type != ResourceType::UNKNOWN && metadata.type != type)
        {
            continue;
        }

        if (MatchesPattern(location.ToString(), pattern))
        {
            results.push_back(location);
        }
    }

    return results;
}

void ResourceSubsystem::ClearAllResources()
{
    std::unique_lock<std::shared_mutex> lock(m_resourceMutex);
    m_preloadedResources.clear();

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] All resources cleared." << '\n';
    }
}

void ResourceSubsystem::UnloadResource(const ResourceLocation& location)
{
    std::unique_lock<std::shared_mutex> lock(m_resourceMutex);
    m_preloadedResources.erase(location);
}

ResourceSubsystem::ResourceStats ResourceSubsystem::GetResourceStats() const
{
    ResourceStats stats;

    {
        std::shared_lock<std::shared_mutex> lock(m_resourceMutex);
        stats.resourceCount = m_preloadedResources.size();

        for (const auto& [loc, resource] : m_preloadedResources)
        {
            if (resource)
            {
                stats.totalSize += resource->GetRawDataSize();
            }
        }
    }

    stats.totalLoaded = m_totalLoaded.load();

    return stats;
}

// ResourceStats reset is not needed since we only track current state

void ResourceSubsystem::ScanResources(std::function<void(const std::string&, size_t)> callback)
{
    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Scanning resources..." << '\n';
    }

    UpdateResourceIndex();

    if (callback)
    {
        callback("Complete", m_resourceIndex.size());
    }
}

void ResourceSubsystem::ScanNamespace(const std::string& namespaceName)
{
    std::shared_lock<std::shared_mutex> providerLock(m_providerMutex);

    for (const auto& provider : m_resourceProviders)
    {
        auto resources = provider->ListResources(namespaceName);

        std::unique_lock<std::shared_mutex> indexLock(m_indexMutex);
        for (const auto& location : resources)
        {
            auto metadata = provider->GetMetadata(location);
            if (metadata)
            {
                m_resourceIndex[location] = *metadata;
            }
        }
    }
}

size_t ResourceSubsystem::CheckAndReloadModifiedResources()
{
    size_t                        reloadedCount = 0;
    std::vector<ResourceLocation> toReload;

    // Check for modified files
    {
        std::shared_lock<std::shared_mutex> lock(m_indexMutex);
        for (const auto& [location, metadata] : m_resourceIndex)
        {
            if (std::filesystem::exists(metadata.filePath))
            {
                auto currentModTime = std::filesystem::last_write_time(metadata.filePath);

                auto it = m_fileModificationTimes.find(location);
                if (it != m_fileModificationTimes.end() && currentModTime > it->second)
                {
                    toReload.push_back(location);
                }

                m_fileModificationTimes[location] = currentModTime;
            }
        }
    }

    // Reload modified resources
    for (const auto& location : toReload)
    {
        UnloadResource(location);
        GetResource(location);
        reloadedCount++;

        if (m_config.logResourceLoads)
        {
            std::cout << "[ResourceSubsystem] Reloaded: " << location.ToString() << std::endl;
        }
    }

    return reloadedCount;
}

void ResourceSubsystem::InitializeDefaultLoaders()
{
    // Register default loaders
    m_loaderRegistry.RegisterLoader(std::make_shared<RawResourceLoader>());

    // Register JSON model loader
    //  m_loaderRegistry.RegisterLoader(std::make_shared<ModelJsonLoader>());

    // Future: Register other default loaders
    // m_loaderRegistry.RegisterLoader(std::make_shared<TextureLoader>());
    
    // Note: SoundLoader will be registered when AudioSystem is available
    // This is done externally to avoid circular dependencies
}

void ResourceSubsystem::InitializeDefaultProviders()
{
    // Create providers for each configured namespace
    for (const auto& nsEntry : m_config.namespaces)
    {
        std::filesystem::path providerPath = nsEntry.customPath.empty() ? m_config.baseAssetPath / nsEntry.name : nsEntry.customPath;

        if (std::filesystem::exists(providerPath))
        {
            auto provider = std::make_shared<FileSystemResourceProvider>(providerPath, nsEntry.name + "Provider");
            provider->SetNamespaceMapping(nsEntry.name, providerPath);
            AddResourceProvider(provider);
        }
        else if (m_config.logResourceLoads)
        {
            std::cout << "[ResourceSubsystem] Warning: Namespace path does not exist: " << providerPath << std::endl;
        }
    }
}

void ResourceSubsystem::StartWorkerThreads()
{
    m_running          = true;
    size_t threadCount = std::min(m_config.loadThreadCount, (size_t)std::thread::hardware_concurrency());

    for (size_t i = 0; i < threadCount; ++i)
    {
        m_workerThreads.emplace_back(&ResourceSubsystem::WorkerThreadFunc, this);
    }

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Started " << threadCount << " worker threads." << std::endl;
    }
}

void ResourceSubsystem::StopWorkerThreads()
{
    m_running = false;
    m_queueCV.notify_all();

    for (auto& thread : m_workerThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    m_workerThreads.clear();
}

void ResourceSubsystem::WorkerThreadFunc()
{
    while (m_running)
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCV.wait(lock, [this] { return !m_loadQueue.empty() || !m_running; });

        if (!m_running) break;

        if (!m_loadQueue.empty())
        {
            LoadRequest request = std::move(m_loadQueue.front());
            m_loadQueue.pop();
            lock.unlock();

            try
            {
                auto resource = LoadResourceInternal(request.location);
                request.promise.set_value(resource);
            }
            catch (const std::exception& e)
            {
                UNUSED(e)
                request.promise.set_exception(std::current_exception());
            }
        }
    }
}

ResourcePtr ResourceSubsystem::LoadResourceInternal(const ResourceLocation& location)
{
    // Debug logging to track namespace corruption
    DebuggerPrintf("[RESOURCE DEBUG] LoadResourceInternal called with namespace='%s', path='%s', toString='%s'\n", 
                   location.GetNamespace().c_str(), location.GetPath().c_str(), location.ToString().c_str());

    // Find provider
    auto provider = FindProviderForResource(location);
    if (!provider)
    {
        throw std::runtime_error("Resource not found: " + location.ToString());
    }

    // Get metadata
    auto metadataOpt = provider->GetMetadata(location);
    if (!metadataOpt)
    {
        throw std::runtime_error("Failed to get metadata for: " + location.ToString());
    }

    // Read data
    auto data = provider->ReadResource(location);
    m_perfStats.bytesLoadedThisFrame += data.size();

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Loading: " << location.ToString()
            << " (" << data.size() << " bytes)" << std::endl;
    }

    // Find loader
    auto loader = m_loaderRegistry.FindLoaderForResource(*metadataOpt);
    if (!loader)
    {
        if (m_config.logResourceLoads)
        {
            std::cout << "[ResourceSubsystem] No specific loader found for " << location.ToString() 
                      << " (extension: " << metadataOpt->GetFileExtension() << "), using RawResourceLoader" << std::endl;
        }
        loader = std::make_shared<RawResourceLoader>();
    }
    else
    {
        if (m_config.logResourceLoads)
        {
            std::cout << "[ResourceSubsystem] Using loader: " << loader->GetLoaderName() 
                      << " for " << location.ToString() << " (extension: " << metadataOpt->GetFileExtension() << ")" << std::endl;
        }
    }

    // Load resource
    ResourcePtr resource = loader->Load(*metadataOpt, data);

    // Store resource in preloaded resources (if not already there)
    {
        std::unique_lock<std::shared_mutex> lock(m_resourceMutex);
        if (m_preloadedResources.find(location) == m_preloadedResources.end())
        {
            m_preloadedResources[location] = resource;
            m_totalLoaded.fetch_add(1);
        }
    }

    // Update file modification time for hot reload
    if (std::filesystem::exists(metadataOpt->filePath))
    {
        m_fileModificationTimes[location] = std::filesystem::last_write_time(metadataOpt->filePath);
    }

    return resource;
}

void ResourceSubsystem::UpdateResourceIndex()
{
    std::unique_lock<std::shared_mutex> indexLock(m_indexMutex);
    std::shared_lock<std::shared_mutex> providerLock(m_providerMutex);

    m_resourceIndex.clear();

    for (const auto& provider : m_resourceProviders)
    {
        auto resources = provider->ListResources();
        for (const auto& location : resources)
        {
            auto metadata = provider->GetMetadata(location);
            if (metadata)
            {
                m_resourceIndex[location] = *metadata;
            }
        }
    }
}

std::shared_ptr<IResourceProvider> ResourceSubsystem::FindProviderForResource(
    const ResourceLocation& location) const
{
    std::shared_lock<std::shared_mutex> lock(m_providerMutex);

    for (const auto& provider : m_resourceProviders)
    {
        if (provider->HasResource(location))
        {
            return provider;
        }
    }

    return nullptr;
}

bool ResourceSubsystem::MatchesPattern(const std::string& str, const std::string& pattern) const
{
    // Simple wildcard matching
    std::string regexPattern = pattern;

    // Escape special characters
    regexPattern = std::regex_replace(regexPattern, std::regex("\\."), "\\.");
    regexPattern = std::regex_replace(regexPattern, std::regex("\\+"), "\\+");

    // Convert wildcards
    regexPattern = std::regex_replace(regexPattern, std::regex("\\*"), ".*");
    regexPattern = std::regex_replace(regexPattern, std::regex("\\?"), ".");

    std::regex re(regexPattern);
    return std::regex_match(str, re);
}

void ResourceSubsystem::PreloadAllDiscoveredResources()
{
    std::vector<ResourceLocation> toPreload;

    {
        std::shared_lock<std::shared_mutex> lock(m_indexMutex);
        for (const auto& [location, metadata] : m_resourceIndex)
        {
            toPreload.push_back(location);
        }
    }

    if (!toPreload.empty())
    {
        if (m_config.logResourceLoads)
        {
            std::cout << "[ResourceSubsystem] Preloading all " << toPreload.size()
                << " discovered resources..." << std::endl;
        }

        size_t loaded = 0;
        size_t total = toPreload.size();
        
        for (const auto& location : toPreload)
        {
            try
            {
                auto resource = LoadResourceInternal(location);
                loaded++;
                
                if (m_config.logResourceLoads && loaded % 10 == 0)
                {
                    std::cout << "[ResourceSubsystem] Loaded " << loaded << "/" << total << " resources" << std::endl;
                }
            }
            catch (const std::exception& e)
            {
                if (m_config.logResourceLoads)
                {
                    std::cout << "[ResourceSubsystem] Failed to load resource: " << location.ToString() << " - " << e.what() << std::endl;
                }
            }
        }
        
        if (m_config.logResourceLoads)
        {
            std::cout << "[ResourceSubsystem] Preloading complete. Loaded " << loaded << "/" << total << " resources." << std::endl;
        }
    }
}

void ResourceSubsystem::UpdateFrameStatistics()
{
    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_frameStartTime).count();

    UNUSED(elapsed)

    // Performance limits disabled - resources load immediately like Minecraft Neoforge
    m_perfStats.isLoadLimited = false;

    m_perfStats.activeLoadThreads = 0;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_perfStats.asyncQueueSize = m_loadQueue.size();
    }
}

bool ResourceSubsystem::ShouldStopLoadingThisFrame() const
{
    // Performance limits disabled - never stop loading resources in a frame
    return false;
}

// Preload all discovered resources method implementation moved above
