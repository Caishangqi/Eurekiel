#pragma once


//-----------------------------------------------------------------------------------------------
#include "ThirdParty/fmod/fmod.h"

#include <string>
#include <vector>
#include <map>


struct Vec3;

namespace FMOD
{
    class Sound;
    class System;
}

//-----------------------------------------------------------------------------------------------
using SoundID                     = size_t;
using SoundPlaybackID             = size_t;
constexpr size_t MISSING_SOUND_ID = static_cast<size_t>(-1); // for bad SoundIDs and SoundPlaybackIDs


//-----------------------------------------------------------------------------------------------
class AudioSystem;

struct AudioSystemConfig
{
};

/////////////////////////////////////////////////////////////////////////////////////////////////
class AudioSystem
{
public:
    AudioSystem();
    AudioSystem(const AudioSystemConfig& audio_config);
    virtual ~AudioSystem();

    void         Startup();
    void         Shutdown();
    virtual void BeginFrame();
    virtual void EndFrame();

    virtual SoundID         CreateOrGetSound(const std::string& soundFilePath, FMOD_INITFLAGS flags = FMOD_3D);
    virtual SoundPlaybackID StartSound(SoundID soundID, bool isLooped = false, float volume   = 1.f, float balance = 0.0f,
                                       float   speed                  = 1.0f, bool   isPaused = false);
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

protected:
    FMOD::System*                  m_fmodSystem;
    std::map<std::string, SoundID> m_registeredSoundIDs;
    std::vector<FMOD::Sound*>      m_registeredSounds;

private:
    AudioSystemConfig m_audioConfig;
};
