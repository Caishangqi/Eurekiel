#include "SoundLoader.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <ThirdParty/json/json.hpp>
#include <fstream>

using namespace enigma::resource;

#if !defined(ENGINE_DISABLE_AUDIO)

SoundLoader::SoundLoader(AudioSystem* audioSystem)
    : m_audioSystem(audioSystem)
{
    // Set up default configuration
    m_defaultConfig.volume = 1.0f;
    m_defaultConfig.pitch = 1.0f;
    m_defaultConfig.weight = 1;
    m_defaultConfig.preload = false;
    m_defaultConfig.loop = false;
    m_defaultConfig.is3D = false;
    m_defaultConfig.minDistance = 1.0f;
    m_defaultConfig.maxDistance = 16.0f;
    m_defaultConfig.stream = false;
}

ResourcePtr SoundLoader::Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data)
{
    if (!m_audioSystem)
    {
        ERROR_RECOVERABLE("SoundLoader: No AudioSystem provided");
        return nullptr;
    }

    if (data.empty())
    {
        ERROR_RECOVERABLE("SoundLoader: Empty sound data for " + metadata.location.ToString());
        return nullptr;
    }

    // Load sound configuration (check for companion .json file)
    SoundResource::SoundConfig config = LoadSoundConfig(metadata);
    
    // Determine FMOD creation flags
    FMOD_MODE mode = GetFMODModeFromConfig(config);
    
    // Create FMOD sound from memory
    FMOD_CREATESOUNDEXINFO exinfo = {};
    exinfo.cbsize = sizeof(FMOD_CREATESOUNDEXINFO);
    exinfo.length = static_cast<unsigned int>(data.size());
    
    FMOD::Sound* fmodSound = nullptr;
    FMOD::System* fmodSystem = m_audioSystem->m_fmodSystem; // Access FMOD system through AudioSystem
    
    if (!fmodSystem)
    {
        ERROR_RECOVERABLE("SoundLoader: FMOD system not initialized");
        return nullptr;
    }
    
    FMOD_RESULT result = fmodSystem->createSound(
        reinterpret_cast<const char*>(data.data()),
        mode | FMOD_OPENMEMORY,
        &exinfo,
        &fmodSound
    );
    
    if (result != FMOD_OK || !fmodSound)
    {
        ERROR_RECOVERABLE("SoundLoader: Failed to create FMOD sound for " + metadata.location.ToString());
        return nullptr;
    }
    
    // Create and return SoundResource
    return std::make_shared<SoundResource>(metadata, fmodSound, config);
}

std::set<std::string> SoundLoader::GetSupportedExtensions() const
{
    return {
        ".wav", ".wave",    // WAV files
        ".mp3",             // MP3 files  
        ".ogg",             // OGG Vorbis files
        ".flac",            // FLAC files
        ".aiff", ".aif",    // AIFF files
        ".m4a", ".mp4",     // M4A/MP4 audio
        ".wma"              // Windows Media Audio
    };
}

bool SoundLoader::CanLoad(const ResourceMetadata& metadata) const
{
    auto extensions = GetSupportedExtensions();
    std::string ext = metadata.GetFileExtension();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return extensions.find(ext) != extensions.end();
}

SoundResource::SoundConfig SoundLoader::LoadSoundConfig(const ResourceMetadata& metadata)
{
    // Start with default configuration
    SoundResource::SoundConfig config = m_defaultConfig;
    
    // Look for companion .json configuration file
    std::filesystem::path configPath = metadata.filePath;
    configPath.replace_extension(".json");
    
    if (std::filesystem::exists(configPath))
    {
        try
        {
            config = SoundConfigLoader::LoadFromFile(configPath);
        }
        catch (const std::exception& e)
        {
            ERROR_RECOVERABLE("SoundLoader: Failed to load sound config from " + configPath.string() + ": " + e.what());
        }
    }
    else
    {
        // Apply heuristics based on path and extension
        std::string path = metadata.location.GetPath();
        std::string ext = metadata.GetFileExtension();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        // UI sounds
        if (path.find("ui/") == 0 || path.find("gui/") == 0)
        {
            config = SoundConfigLoader::GetUIConfig();
        }
        // Music
        else if (path.find("music/") == 0 || path.find("bgm/") == 0)
        {
            config = SoundConfigLoader::GetMusicConfig();
        }
        // Ambient sounds
        else if (path.find("ambient/") == 0 || path.find("environment/") == 0)
        {
            config = SoundConfigLoader::GetAmbientConfig();
        }
        // Sound effects
        else if (path.find("sfx/") == 0 || path.find("effects/") == 0 || path.find("sounds/") == 0)
        {
            config = SoundConfigLoader::GetEffectConfig();
        }
        
        // Enable streaming for large files or specific formats
        if (IsStreamingFormat(ext))
        {
            config.stream = true;
        }
    }
    
    return config;
}

FMOD_MODE SoundLoader::GetFMODModeFromConfig(const SoundResource::SoundConfig& config)
{
    FMOD_MODE mode = FMOD_DEFAULT;
    
    if (config.stream)
        mode |= FMOD_CREATESTREAM;
    else
        mode |= FMOD_CREATESAMPLE;
        
    if (config.loop)
        mode |= FMOD_LOOP_NORMAL;
    else
        mode |= FMOD_LOOP_OFF;
        
    if (config.is3D)
        mode |= FMOD_3D;
    else
        mode |= FMOD_2D;
    
    return mode;
}

bool SoundLoader::IsStreamingFormat(const std::string& extension)
{
    // Enable streaming for formats that are typically large or benefit from streaming
    return extension == ".mp3" || extension == ".ogg" || extension == ".flac" || 
           extension == ".m4a" || extension == ".mp4" || extension == ".wma";
}

// SoundConfigLoader implementation
SoundResource::SoundConfig SoundConfigLoader::LoadFromJson(const std::string& jsonContent)
{
    SoundResource::SoundConfig config;
    
    try
    {
        auto json = nlohmann::json::parse(jsonContent);
        
        // Parse JSON properties
        if (json.contains("stream"))
            config.stream = json["stream"].get<bool>();
        if (json.contains("volume"))
            config.volume = json["volume"].get<float>();
        if (json.contains("pitch"))
            config.pitch = json["pitch"].get<float>();
        if (json.contains("weight"))
            config.weight = json["weight"].get<int>();
        if (json.contains("preload"))
            config.preload = json["preload"].get<bool>();
        if (json.contains("loop"))
            config.loop = json["loop"].get<bool>();
        if (json.contains("is3D"))
            config.is3D = json["is3D"].get<bool>();
        if (json.contains("minDistance"))
            config.minDistance = json["minDistance"].get<float>();
        if (json.contains("maxDistance"))
            config.maxDistance = json["maxDistance"].get<float>();
    }
    catch (const std::exception& e)
    {
        ERROR_RECOVERABLE("SoundConfigLoader: JSON parsing error: " + std::string(e.what()));
    }
    
    return config;
}

SoundResource::SoundConfig SoundConfigLoader::LoadFromFile(const std::filesystem::path& configPath)
{
    std::ifstream file(configPath);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open config file: " + configPath.string());
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    return LoadFromJson(content);
}

SoundResource::SoundConfig SoundConfigLoader::GetUIConfig()
{
    SoundResource::SoundConfig config;
    config.stream = false;      // UI sounds are typically short
    config.volume = 1.0f;
    config.pitch = 1.0f;
    config.weight = 1;
    config.preload = true;      // Preload UI sounds for responsiveness
    config.loop = false;
    config.is3D = false;        // UI sounds are 2D
    config.minDistance = 1.0f;
    config.maxDistance = 16.0f;
    return config;
}

SoundResource::SoundConfig SoundConfigLoader::GetMusicConfig()
{
    SoundResource::SoundConfig config;
    config.stream = true;       // Music is typically large and should be streamed
    config.volume = 0.8f;       // Slightly lower default volume for music
    config.pitch = 1.0f;
    config.weight = 1;
    config.preload = false;     // Don't preload large music files
    config.loop = true;         // Music often loops
    config.is3D = false;        // Music is usually 2D
    config.minDistance = 1.0f;
    config.maxDistance = 16.0f;
    return config;
}

SoundResource::SoundConfig SoundConfigLoader::GetAmbientConfig()
{
    SoundResource::SoundConfig config;
    config.stream = true;       // Ambient sounds can be long
    config.volume = 0.6f;       // Lower volume for ambient sounds
    config.pitch = 1.0f;
    config.weight = 1;
    config.preload = false;
    config.loop = true;         // Ambient sounds typically loop
    config.is3D = true;         // Ambient sounds are usually positional
    config.minDistance = 2.0f;
    config.maxDistance = 32.0f; // Larger range for ambient sounds
    return config;
}

SoundResource::SoundConfig SoundConfigLoader::GetEffectConfig()
{
    SoundResource::SoundConfig config;  
    config.stream = false;      // Sound effects are typically short
    config.volume = 1.0f;
    config.pitch = 1.0f;
    config.weight = 1;
    config.preload = true;      // Preload common sound effects
    config.loop = false;
    config.is3D = true;         // Sound effects are often positional
    config.minDistance = 1.0f;
    config.maxDistance = 16.0f;
    return config;
}

#else // ENGINE_DISABLE_AUDIO

// Dummy implementations when audio is disabled
SoundLoader::SoundLoader(AudioSystem* audioSystem)
    : m_audioSystem(audioSystem)
{
}

ResourcePtr SoundLoader::Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data)
{
    UNUSED(metadata); UNUSED(data);
    return nullptr;
}

std::set<std::string> SoundLoader::GetSupportedExtensions() const
{
    return {};
}

bool SoundLoader::CanLoad(const ResourceMetadata& metadata) const
{
    UNUSED(metadata);
    return false;
}

SoundResource::SoundConfig SoundLoader::LoadSoundConfig(const ResourceMetadata& metadata)
{
    UNUSED(metadata);
    return SoundResource::SoundConfig{};
}

FMOD_MODE SoundLoader::GetFMODModeFromConfig(const SoundResource::SoundConfig& config)
{
    UNUSED(config);
    return FMOD_DEFAULT;
}

bool SoundLoader::IsStreamingFormat(const std::string& extension)
{
    UNUSED(extension);
    return false;
}

// SoundConfigLoader dummy implementations
SoundResource::SoundConfig SoundConfigLoader::LoadFromJson(const std::string& jsonContent)
{
    UNUSED(jsonContent);
    return SoundResource::SoundConfig{};
}

SoundResource::SoundConfig SoundConfigLoader::LoadFromFile(const std::filesystem::path& configPath)
{
    UNUSED(configPath);
    return SoundResource::SoundConfig{};
}

SoundResource::SoundConfig SoundConfigLoader::GetUIConfig()
{
    return SoundResource::SoundConfig{};
}

SoundResource::SoundConfig SoundConfigLoader::GetMusicConfig()
{
    return SoundResource::SoundConfig{};
}

SoundResource::SoundConfig SoundConfigLoader::GetAmbientConfig()
{
    return SoundResource::SoundConfig{};
}

SoundResource::SoundConfig SoundConfigLoader::GetEffectConfig()
{
    return SoundResource::SoundConfig{};
}

#endif // !defined(ENGINE_DISABLE_AUDIO)

