#pragma once
#include "SoundResource.hpp"
#include "../ResourceLoader.hpp"
#include "Engine/Audio/AudioSubsystem.hpp"
#include <set>

namespace enigma::resource
{
    /**
     * @brief Sound resource loader implementing the Neoforge sound loading pattern
     * 
     * This loader handles various audio formats supported by FMOD and creates
     * SoundResource instances with appropriate configuration.
     * 
     * Supported formats: WAV, MP3, OGG, FLAC, AIFF, and others supported by FMOD
     */
    class SoundLoader : public IResourceLoader
    {
    public:
        explicit SoundLoader(AudioSubsystem* audioSystem = nullptr);
        virtual  ~SoundLoader() = default;

        // IResourceLoader interface
        ResourcePtr           Load(const ResourceMetadata& metadata, const std::vector<uint8_t>& data) override;
        std::set<std::string> GetSupportedExtensions() const override;
        std::string           GetLoaderName() const override { return "SoundLoader"; }
        int                   GetPriority() const override { return 100; } // High priority for sound files
        bool                  CanLoad(const ResourceMetadata& metadata) const override;

        // Sound-specific configuration
        void                              SetDefaultConfig(const SoundResource::SoundConfig& config) { m_defaultConfig = config; }
        const SoundResource::SoundConfig& GetDefaultConfig() const { return m_defaultConfig; }

        // AudioSubsystem management
        void         SetAudioSystem(AudioSubsystem* audioSystem) { m_audioSystem = audioSystem; }
        AudioSubsystem* GetAudioSystem() const { return m_audioSystem; }

    private:
        AudioSubsystem*               m_audioSystem;
        SoundResource::SoundConfig m_defaultConfig;

        // Helper methods
        SoundResource::SoundConfig LoadSoundConfig(const ResourceMetadata& metadata);
        FMOD_MODE                  GetFMODModeFromConfig(const SoundResource::SoundConfig& config);
        bool                       IsStreamingFormat(const std::string& extension);
    };


    /**
     * @brief JSON-based sound configuration loader (similar to Neoforge sounds.json)
     * 
     * This loader can parse JSON configuration files that define sound properties,
     * similar to how Neoforge handles sounds.json files.
     */
    class SoundConfigLoader
    {
    public:
        static SoundResource::SoundConfig LoadFromJson(const std::string& jsonContent);
        static SoundResource::SoundConfig LoadFromFile(const std::filesystem::path& configPath);

        // Default configurations for different sound types
        static SoundResource::SoundConfig GetUIConfig(); // UI sounds (2D, short, no streaming)
        static SoundResource::SoundConfig GetMusicConfig(); // Music (streaming, looped)
        static SoundResource::SoundConfig GetAmbientConfig(); // Ambient sounds (3D, looped)
        static SoundResource::SoundConfig GetEffectConfig(); // Sound effects (3D, short)
    };
}
