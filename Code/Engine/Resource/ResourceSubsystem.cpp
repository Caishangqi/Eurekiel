#include "ResourceSubsystem.hpp"

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
        hotReloadCheckInterval > 0 &&
        performanceLimits.maxLoadAttemptsPerFrame > 0 &&
        performanceLimits.maxBytesPerFrame > 0 &&
        performanceLimits.maxLoadTimePerFrame > 0;
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

    // Process namespace preloads
    ProcessNamespacePreloads();

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
    ClearCache();

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
    // Check cache first
    {
        std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
        auto                                it = m_resourceCache.find(location);
        if (it != m_resourceCache.end())
        {
            // Update access time and count
            const_cast<CacheEntry&>(it->second).lastAccess = std::chrono::steady_clock::now();
            const_cast<CacheEntry&>(it->second).accessCount++;

            m_cacheHits.fetch_add(1);
            return it->second.resource;
        }
    }

    m_cacheMisses.fetch_add(1);

    if (m_config.logCacheMisses)
    {
        std::cout << "[ResourceSubsystem] Cache miss: " << location.ToString() << std::endl;
    }

    // Check if we should stop loading this frame
    if (ShouldStopLoadingThisFrame())
    {
        m_perfStats.isLoadLimited = true;
        return nullptr;
    }

    // Load resource
    return LoadResourceInternal(location);
}

std::future<ResourcePtr> ResourceSubsystem::GetResourceAsync(const ResourceLocation& location)
{
    // Check cache first
    {
        std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
        auto                                it = m_resourceCache.find(location);
        if (it != m_resourceCache.end())
        {
            m_cacheHits.fetch_add(1);
            std::promise<ResourcePtr> promise;
            promise.set_value(it->second.resource);
            return promise.get_future();
        }
    }

    m_cacheMisses.fetch_add(1);

    // Get estimated size for the resource
    size_t estimatedSize = 0;
    {
        std::shared_lock<std::shared_mutex> lock(m_indexMutex);
        auto                                it = m_resourceIndex.find(location);
        if (it != m_resourceIndex.end())
        {
            estimatedSize = it->second.fileSize;
        }
    }

    // Add to load queue
    LoadRequest request;
    request.location      = location;
    request.requestTime   = std::chrono::steady_clock::now();
    request.estimatedSize = estimatedSize;
    auto future           = request.promise.get_future();

    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_loadQueue.push(std::move(request));
    }
    m_queueCV.notify_one();

    return future;
}

void ResourceSubsystem::PreloadResources(const std::vector<ResourceLocation>& locations,
                                         std::function<void(size_t, size_t)>  callback)
{
    size_t total  = locations.size();
    size_t loaded = 0;

    for (const auto& location : locations)
    {
        // Check if resource should be loaded async based on size
        bool useAsync = false;
        {
            std::shared_lock<std::shared_mutex> lock(m_indexMutex);
            auto                                it = m_resourceIndex.find(location);
            if (it != m_resourceIndex.end())
            {
                useAsync = it->second.fileSize >= m_config.minFileSizeForAsync;
            }
        }

        if (useAsync)
        {
            GetResourceAsync(location);
        }
        else
        {
            GetResource(location);
        }

        loaded++;
        if (callback)
        {
            callback(loaded, total);
        }
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

void ResourceSubsystem::ClearCache()
{
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    m_resourceCache.clear();

    if (m_config.logResourceLoads)
    {
        std::cout << "[ResourceSubsystem] Cache cleared." << '\n';
    }
}

void ResourceSubsystem::UnloadResource(const ResourceLocation& location)
{
    std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
    m_resourceCache.erase(location);
}

ResourceSubsystem::CacheStats ResourceSubsystem::GetCacheStats() const
{
    CacheStats stats;

    {
        std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
        stats.resourceCount = m_resourceCache.size();

        for (const auto& [loc, entry] : m_resourceCache)
        {
            if (entry.resource)
            {
                stats.totalSize += entry.resource->GetRawDataSize();
            }
        }
    }

    stats.hitCount      = m_cacheHits.load();
    stats.missCount     = m_cacheMisses.load();
    stats.evictionCount = m_cacheEvictions.load();

    size_t total  = stats.hitCount + stats.missCount;
    stats.hitRate = total > 0 ? (float)stats.hitCount / (float)total * 100.0f : 0.0f;

    return stats;
}

void ResourceSubsystem::ResetCacheStats()
{
    m_cacheHits      = 0;
    m_cacheMisses    = 0;
    m_cacheEvictions = 0;
}

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
    // m_loaderRegistry.RegisterLoader(std::make_shared<AudioLoader>());
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
    // Update performance stats
    auto loadStart = std::chrono::steady_clock::now();
    m_perfStats.loadAttemptsThisFrame++;

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
        loader = std::make_shared<RawResourceLoader>();
    }

    // Load resource
    ResourcePtr resource = loader->Load(*metadataOpt, data);

    // Cache resource
    {
        std::unique_lock<std::shared_mutex> lock(m_cacheMutex);
        CacheEntry                          entry;
        entry.resource            = resource;
        entry.lastAccess          = std::chrono::steady_clock::now();
        entry.accessCount         = 1;
        m_resourceCache[location] = entry;
    }

    // Update file modification time for hot reload
    if (std::filesystem::exists(metadataOpt->filePath))
    {
        m_fileModificationTimes[location] = std::filesystem::last_write_time(metadataOpt->filePath);
    }

    // Update performance stats
    auto loadEnd = std::chrono::steady_clock::now();
    m_perfStats.loadTimeThisFrame += std::chrono::duration<double>(loadEnd - loadStart).count();

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

void ResourceSubsystem::ProcessNamespacePreloads()
{
    for (const auto& nsEntry : m_config.namespaces)
    {
        if (!nsEntry.preloadAll && nsEntry.preloadPatterns.empty())
        {
            continue;
        }

        std::vector<ResourceLocation> toPreload;

        {
            std::shared_lock<std::shared_mutex> lock(m_indexMutex);
            for (const auto& [location, metadata] : m_resourceIndex)
            {
                if (location.GetNamespace() != nsEntry.name)
                {
                    continue;
                }

                bool shouldPreload = nsEntry.preloadAll;

                if (!shouldPreload)
                {
                    for (const auto& pattern : nsEntry.preloadPatterns)
                    {
                        if (MatchesPattern(location.GetPath(), pattern))
                        {
                            shouldPreload = true;
                            break;
                        }
                    }
                }

                if (shouldPreload)
                {
                    toPreload.push_back(location);
                }
            }
        }

        if (!toPreload.empty())
        {
            if (m_config.logResourceLoads)
            {
                std::cout << "[ResourceSubsystem] Preloading " << toPreload.size()
                    << " resources from namespace '" << nsEntry.name << "'" << std::endl;
            }

            PreloadResources(toPreload);
        }
    }
}

void ResourceSubsystem::UpdateFrameStatistics()
{
    auto now     = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<double>(now - m_frameStartTime).count();

    UNUSED(elapsed)

    m_perfStats.isLoadLimited = (
        m_perfStats.loadAttemptsThisFrame >= m_config.performanceLimits.maxLoadAttemptsPerFrame ||
        m_perfStats.bytesLoadedThisFrame >= m_config.performanceLimits.maxBytesPerFrame ||
        m_perfStats.loadTimeThisFrame >= m_config.performanceLimits.maxLoadTimePerFrame
    );

    m_perfStats.activeLoadThreads = 0;
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_perfStats.asyncQueueSize = m_loadQueue.size();
    }
}

bool ResourceSubsystem::ShouldStopLoadingThisFrame() const
{
    if (!m_config.enableParallelLoading)
    {
        return false;
    }

    return m_perfStats.loadAttemptsThisFrame >= m_config.performanceLimits.maxLoadAttemptsPerFrame ||
        m_perfStats.bytesLoadedThisFrame >= m_config.performanceLimits.maxBytesPerFrame ||
        m_perfStats.loadTimeThisFrame >= m_config.performanceLimits.maxLoadTimePerFrame;
}

void ResourceSubsystem::EvictLRUResources()
{
    if (m_config.logCacheEvictions)
    {
        std::cout << "[ResourceSubsystem] Starting cache eviction..." << std::endl;
    }

    std::vector<std::pair<ResourceLocation, std::chrono::steady_clock::time_point>> sortedResources;

    {
        std::shared_lock<std::shared_mutex> lock(m_cacheMutex);
        for (const auto& [location, entry] : m_resourceCache)
        {
            sortedResources.emplace_back(location, entry.lastAccess);
        }
    }

    // Sort by last access time (oldest first)
    std::sort(sortedResources.begin(), sortedResources.end(),
              [](const auto& a, const auto& b)
              {
                  return a.second < b.second;
              });

    // Evict oldest resources until we're under the threshold
    size_t currentSize  = 0;
    size_t evictedCount = 0;

    {
        std::unique_lock<std::shared_mutex> lock(m_cacheMutex);

        // Calculate current size
        for (const auto& [loc, entry] : m_resourceCache)
        {
            if (entry.resource)
            {
                currentSize += entry.resource->GetRawDataSize();
            }
        }

        // Evict resources
        for (const auto& [location, lastAccess] : sortedResources)
        {
            if (currentSize <= m_config.maxCacheSize * m_config.cacheEvictionThreshold)
            {
                break;
            }

            auto it = m_resourceCache.find(location);
            if (it != m_resourceCache.end())
            {
                currentSize -= it->second.resource->GetRawDataSize();
                m_resourceCache.erase(it);
                evictedCount++;
                m_cacheEvictions.fetch_add(1);
            }
        }
    }

    if (m_config.logCacheEvictions)
    {
        std::cout << "[ResourceSubsystem] Evicted " << evictedCount << " resources." << std::endl;
    }
}
