#pragma once

//-----------------------------------------------------------------------------------------------
#include "ThirdParty/fmod/fmod.h"
#include "../Core/SubsystemManager.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>

// Undefine Windows API macros that conflict with our method names
#ifdef PlaySound
#undef PlaySound
#endif

struct Vec3;

namespace FMOD
{
    class Sound;
    class System;
}

// Forward declarations for resource system integration
namespace enigma::resource
{
    class ResourceSubsystem;
    class SoundResource;
    class ResourceLocation;
}

//-----------------------------------------------------------------------------------------------
using SoundID                     = size_t;
using SoundPlaybackID             = size_t;
constexpr size_t MISSING_SOUND_ID = static_cast<size_t>(-1); // for bad SoundIDs and SoundPlaybackIDs


//-----------------------------------------------------------------------------------------------
class AudioSubsystem;

struct AudioSystemConfig
{
    bool                                 enableResourceIntegration = true; // Enable resource system integration
    enigma::resource::ResourceSubsystem* resourceSubsystem         = nullptr;
};

/////////////////////////////////////////////////////////////////////////////////////////////////
class AudioSubsystem : public enigma::core::EngineSubsystem
{
public:
    AudioSubsystem();
    AudioSubsystem(const AudioSystemConfig& audio_config);
    ~AudioSubsystem() override;

    // EngineSubsystem interface
    DECLARE_SUBSYSTEM(AudioSubsystem, "audio", 50)

    void Initialize() override; // Early initialization: FMOD setup + SoundLoader registration
    void Startup() override; // Main startup: Continue with existing startup logic
    void Shutdown() override;
    void BeginFrame() override;
    void EndFrame() override;
    bool RequiresInitialize() const override { return true; } // AudioSubsystem needs early init

    // Legacy API (file path based)
    virtual SoundID         CreateOrGetSound(const std::string& soundFilePath, FMOD_INITFLAGS flags = FMOD_3D);
    virtual SoundPlaybackID StartSound(SoundID soundID, bool isLooped = false, float volume   = 1.f, float balance = 0.0f,
                                       float   speed                  = 1.0f, bool   isPaused = false);

    std::shared_ptr<enigma::resource::SoundResource> LoadSound(const enigma::resource::ResourceLocation& location);
    SoundPlaybackID PlaySound(const enigma::resource::ResourceLocation& location, bool isLooped = false, float volume = 1.0f, float balance = 0.0f, float speed = 1.0f, bool isPaused = false);
    SoundPlaybackID PlaySoundAt(const enigma::resource::ResourceLocation& location, const Vec3& position, bool isLooped = false, float volume = 1.0f, float balance = 0.0f, float speed = 1.0f,
                                bool isPaused = false);

    // Playback control
    virtual void StopSound(SoundPlaybackID soundPlaybackID);
    virtual void SetSoundPlaybackVolume(SoundPlaybackID soundPlaybackID, float volume); // volume is in [0,1]
    virtual void SetSoundPlaybackBalance(SoundPlaybackID soundPlaybackID, float balance);
    // balance is in [-1,1], where 0 is L/R centered
    virtual void SetSoundPlaybackSpeed(SoundPlaybackID soundPlaybackID, float speed);
    // speed is frequency multiplier (1.0 == normal)

    virtual void ValidateResult(FMOD_RESULT result);

    /// Sound 3D support
    void                    SetNumListeners(int numListeners);
    void                    UpdateListener(int listenerIndex, const Vec3& listenerPosition, const Vec3& listenerForward, const Vec3& listenerUp);
    virtual SoundPlaybackID StartSoundAt(SoundID soundID, const Vec3& soundPosition, bool isLooped = false, float volume = 1.0f, float balance = 0.0f, float speed = 1.0f, bool isPaused = false);
    virtual void            SetSoundPosition(SoundPlaybackID soundPlaybackID, const Vec3& soundPosition);
    bool                    IsPlaying(SoundPlaybackID soundPlaybackID);

private:
    static FMOD_VECTOR GameToFmodVec(const Vec3& v);

public:
    // Public access to FMOD system for resource loaders
    FMOD::System* m_fmodSystem;

protected:
    std::map<std::string, SoundID> m_registeredSoundIDs;
    std::vector<FMOD::Sound*>      m_registeredSounds;

    // Resource system integration
    enigma::resource::ResourceSubsystem* m_resourceSubsystem = nullptr;

private:
    AudioSystemConfig m_audioConfig;
};
