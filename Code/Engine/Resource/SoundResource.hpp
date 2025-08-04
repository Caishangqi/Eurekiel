#pragma once
#include "ResourceMetadata.hpp"
#include "Engine/Audio/AudioSystem.hpp"
#include "ThirdParty/fmod/fmod.hpp"
#include <memory>

namespace enigma::resource
{
    /**
     * @brief Sound resource representation following Neoforge sound system design
     * 
     * This class encapsulates FMOD sound data and provides a resource-based interface
     * similar to Minecraft Neoforge's sound system architecture.
     * 
     * Reference: https://docs.neoforged.net/docs/resources/client/sounds
     */
    class SoundResource : public IResource
    {
    public:
        /**
         * @brief Sound configuration structure similar to Neoforge sound.json
         */
        struct SoundConfig
        {
            bool stream = false;        // Whether to stream the sound instead of loading into memory
            float volume = 1.0f;        // Default volume (0.0 - 1.0)
            float pitch = 1.0f;         // Default pitch multiplier
            int weight = 1;             // Weight for random selection in sound groups
            bool preload = false;       // Whether to preload this sound
            bool loop = false;          // Whether this sound should loop by default
            
            // 3D sound properties
            float minDistance = 1.0f;   // Minimum distance for 3D attenuation
            float maxDistance = 16.0f;  // Maximum distance for 3D attenuation
            bool is3D = false;          // Whether this is a 3D positional sound
        };

    public:
        SoundResource(const ResourceMetadata& metadata, FMOD::Sound* fmodSound, const SoundConfig& config = {});
        virtual ~SoundResource();

        // IResource interface implementation
        const ResourceMetadata& GetMetadata() const override { return m_metadata; }
        ResourceType GetType() const override { return ResourceType::SOUND; }
        bool IsLoaded() const override { return m_fmodSound != nullptr; }
        const void* GetRawData() const override;
        size_t GetRawDataSize() const override;

        // Sound-specific interface
        FMOD::Sound* GetFMODSound() const { return m_fmodSound; }
        const SoundConfig& GetConfig() const { return m_config; }
        
        // Convenience methods
        SoundPlaybackID Play(AudioSystem& audioSystem, bool isLooped = false, float volume = 1.0f, float balance = 0.0f, float speed = 1.0f, bool isPaused = false) const;
        SoundPlaybackID PlayAt(AudioSystem& audioSystem, const Vec3& position, bool isLooped = false, float volume = 1.0f, float balance = 0.0f, float speed = 1.0f, bool isPaused = false) const;
        
        // Sound properties
        float GetLength() const;        // Get sound length in seconds
        int GetChannels() const;        // Get number of channels
        int GetFrequency() const;       // Get sample rate
        FMOD_SOUND_FORMAT GetFormat() const; // Get sound format

        // Resource management
        void Unload();
        bool Reload(const std::vector<uint8_t>& data, const SoundConfig& config = {});

    private:
        ResourceMetadata m_metadata;
        FMOD::Sound* m_fmodSound;
        SoundConfig m_config;
        mutable std::vector<uint8_t> m_rawData; // Cache for raw data access
        mutable bool m_rawDataCached = false;
        
        void CacheRawData() const;
    };

    using SoundResourcePtr = std::shared_ptr<SoundResource>;
}