#include "Engine/Audio/AudioSubsystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/Engine.hpp"

//-----------------------------------------------------------------------------------------------
// To disable audio entirely (and remove requirement for fmod.dll / fmod64.dll) for any game,
//	#define ENGINE_DISABLE_AUDIO in your game's Code/Game/EngineBuildPreferences.hpp file.
//
// Note that this #include is an exception to the rule "engine code doesn't know about game code".
//	Purpose: Each game can now direct the engine via #defines to build differently for that game.
//	Downside: ALL games must now have this Code/Game/EngineBuildPreferences.hpp file.
//
// SD1 NOTE: THIS MEANS *EVERY* GAME MUST HAVE AN EngineBuildPreferences.hpp FILE IN ITS CODE/GAME FOLDER!!
#include "Engine/Math/Vec3.hpp"
#include "ThirdParty/fmod/fmod.hpp"
#include "Game/EngineBuildPreferences.hpp"

// Resource system integration
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Engine/Resource/Sound/SoundResource.hpp"
#include "Engine/Resource/Sound/SoundLoader.hpp"

#if !defined( ENGINE_DISABLE_AUDIO )


//-----------------------------------------------------------------------------------------------
// Link in the appropriate FMOD static library (32-bit or 64-bit)
//
#if defined( _WIN64 )
#pragma comment( lib, "ThirdParty/fmod/fmod64_vc.lib" )
#else
#pragma comment( lib, "ThirdParty/fmod/fmod_vc.lib" )
#endif


//-----------------------------------------------------------------------------------------------
// Initialization code based on example from "FMOD Studio Programmers API for Windows"
//
AudioSubsystem::AudioSubsystem()
    : m_fmodSystem(nullptr)
{
}

AudioSubsystem::AudioSubsystem(const AudioSystemConfig& audio_config) : m_fmodSystem(nullptr)
{
    m_audioConfig = audio_config;
}


//-----------------------------------------------------------------------------------------------
AudioSubsystem::~AudioSubsystem()
{
}


//------------------------------------------------------------------------------------------------
void AudioSubsystem::Initialize()
{
    // Phase 1: Early initialization - Create FMOD system and register SoundLoader
    FMOD_RESULT result;
    result = System_Create(&m_fmodSystem);
    ValidateResult(result);
    // When calling fmod init, pass in the flag FMOD_INIT_3D_RIGHTHANDED.
    result = m_fmodSystem->init(512, FMOD_INIT_3D_RIGHTHANDED, nullptr);
    ValidateResult(result);

    // Get ResourceSubsystem dependency from Engine and register SoundLoader
    if (m_audioConfig.enableResourceIntegration)
    {
        auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
        if (resourceSubsystem)
        {
            m_resourceSubsystem = resourceSubsystem;
            auto soundLoader    = std::make_shared<enigma::resource::SoundLoader>(this);
            m_resourceSubsystem->RegisterLoader(soundLoader);
        }
        else
        {
            ERROR_RECOVERABLE("AudioSubsystem: ResourceSubsystem dependency not found!")
        }
    }
}

//------------------------------------------------------------------------------------------------
void AudioSubsystem::Startup()
{
    // Phase 2: Main startup - FMOD is already initialized, just continue with any additional setup
    // Currently no additional setup needed beyond Initialize phase
}


//------------------------------------------------------------------------------------------------
void AudioSubsystem::Shutdown()
{
    FMOD_RESULT result = m_fmodSystem->release();
    ValidateResult(result);

    m_fmodSystem = nullptr; // #Fixme: do we delete/free the object also, or just do this?
}


//-----------------------------------------------------------------------------------------------
void AudioSubsystem::BeginFrame()
{
    m_fmodSystem->update();
}


//-----------------------------------------------------------------------------------------------
void AudioSubsystem::EndFrame()
{
}


//-----------------------------------------------------------------------------------------------
SoundID AudioSubsystem::CreateOrGetSound(const std::string& soundFilePath, FMOD_INITFLAGS flags)
{
    auto found = m_registeredSoundIDs.find(soundFilePath);
    if (found != m_registeredSoundIDs.end())
    {
        return found->second;
    }
    FMOD::Sound* newSound = nullptr;
    m_fmodSystem->createSound(soundFilePath.c_str(), flags, nullptr, &newSound);
    if (newSound)
    {
        SoundID newSoundID                  = m_registeredSounds.size();
        m_registeredSoundIDs[soundFilePath] = newSoundID;
        m_registeredSounds.push_back(newSound);
        return newSoundID;
    }

    return MISSING_SOUND_ID;
}


//-----------------------------------------------------------------------------------------------
SoundPlaybackID AudioSubsystem::StartSound(SoundID soundID, bool isLooped, float volume, float balance, float speed,
                                           bool    isPaused)
{
    size_t numSounds = m_registeredSounds.size();
    if (soundID < 0 || soundID >= numSounds)
        return MISSING_SOUND_ID;

    FMOD::Sound* sound = m_registeredSounds[soundID];
    if (!sound)
        return MISSING_SOUND_ID;

    FMOD::Channel* channelAssignedToSound = nullptr;
    m_fmodSystem->playSound(sound, nullptr, isPaused, &channelAssignedToSound);
    if (channelAssignedToSound)
    {
        int          loopCount    = isLooped ? -1 : 0;
        unsigned int playbackMode = isLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
        float        frequency;
        channelAssignedToSound->setMode(playbackMode);
        channelAssignedToSound->getFrequency(&frequency);
        channelAssignedToSound->setFrequency(frequency * speed);
        channelAssignedToSound->setVolume(volume);
        channelAssignedToSound->setPan(balance);
        channelAssignedToSound->setLoopCount(loopCount);
    }

    return (SoundPlaybackID)channelAssignedToSound;
}


//-----------------------------------------------------------------------------------------------
void AudioSubsystem::StopSound(SoundPlaybackID soundPlaybackID)
{
    if (soundPlaybackID == MISSING_SOUND_ID)
    {
        ERROR_RECOVERABLE("WARNING: attempt to stop sound on missing sound playback ID!");
        return;
    }

    auto channelAssignedToSound = (FMOD::Channel*)soundPlaybackID;
    channelAssignedToSound->stop();
}


//-----------------------------------------------------------------------------------------------
// Volume is in [0,1]
//
void AudioSubsystem::SetSoundPlaybackVolume(SoundPlaybackID soundPlaybackID, float volume)
{
    if (soundPlaybackID == MISSING_SOUND_ID)
    {
        ERROR_RECOVERABLE("WARNING: attempt to set volume on missing sound playback ID!");
        return;
    }

    auto channelAssignedToSound = (FMOD::Channel*)soundPlaybackID;
    channelAssignedToSound->setVolume(volume);
}


//-----------------------------------------------------------------------------------------------
// Balance is in [-1,1], where 0 is L/R centered
//
void AudioSubsystem::SetSoundPlaybackBalance(SoundPlaybackID soundPlaybackID, float balance)
{
    if (soundPlaybackID == MISSING_SOUND_ID)
    {
        ERROR_RECOVERABLE("WARNING: attempt to set balance on missing sound playback ID!");
        return;
    }

    auto channelAssignedToSound = (FMOD::Channel*)soundPlaybackID;
    channelAssignedToSound->setPan(balance);
}


//-----------------------------------------------------------------------------------------------
// Speed is frequency multiplier (1.0 == normal)
//	A speed of 2.0 gives 2x frequency, i.e. exactly one octave higher
//	A speed of 0.5 gives 1/2 frequency, i.e. exactly one octave lower
//
void AudioSubsystem::SetSoundPlaybackSpeed(SoundPlaybackID soundPlaybackID, float speed)
{
    if (soundPlaybackID == MISSING_SOUND_ID)
    {
        ERROR_RECOVERABLE("WARNING: attempt to set speed on missing sound playback ID!");
        return;
    }

    auto         channelAssignedToSound = (FMOD::Channel*)soundPlaybackID;
    float        frequency;
    FMOD::Sound* currentSound = nullptr;
    channelAssignedToSound->getCurrentSound(&currentSound);
    if (!currentSound)
        return;

    int ignored = 0;
    currentSound->getDefaults(&frequency, &ignored);
    channelAssignedToSound->setFrequency(frequency * speed);
}


//-----------------------------------------------------------------------------------------------
void AudioSubsystem::ValidateResult(FMOD_RESULT result)
{
    if (result != FMOD_OK)
    {
        ERROR_RECOVERABLE(
            Stringf( "Engine/Audio SYSTEM ERROR: Got error result code %i - error codes listed in fmod_common.h\n",
                result ));
    }
}

void AudioSubsystem::SetNumListeners(int numListeners)
{
    FMOD_RESULT r = m_fmodSystem->set3DNumListeners(numListeners);
    ValidateResult(r);
}

void AudioSubsystem::UpdateListener(int listenerIndex, const Vec3& listenerPosition, const Vec3& listenerForward, const Vec3& listenerUp)
{
    FMOD_VECTOR pos     = GameToFmodVec(listenerPosition);
    FMOD_VECTOR vel     = {0.f, 0.f, 0.f};
    FMOD_VECTOR forward = GameToFmodVec(listenerForward);
    FMOD_VECTOR up      = GameToFmodVec(listenerUp);

    FMOD_RESULT r = m_fmodSystem->set3DListenerAttributes(
        listenerIndex, &pos, &vel, &forward, &up);
    ValidateResult(r);
}

SoundPlaybackID AudioSubsystem::StartSoundAt(SoundID soundID, const Vec3& soundPosition, bool isLooped, float volume, float balance, float speed, bool isPaused)
{
    size_t numSounds = m_registeredSounds.size();
    if (soundID < 0 || soundID >= numSounds)
        return MISSING_SOUND_ID;

    FMOD::Sound* sound = m_registeredSounds[soundID];
    if (!sound)
        return MISSING_SOUND_ID;
    // Pause first, configure properties, then unpause
    FMOD::Channel* chan = nullptr;
    m_fmodSystem->playSound(sound, nullptr, /*paused=*/true, &chan);
    if (!chan) return MISSING_SOUND_ID;

    int          loopCount    = isLooped ? -1 : 0;
    unsigned int playbackMode = isLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
    playbackMode |= FMOD_3D;

    chan->setMode(playbackMode);
    float freq;
    chan->getFrequency(&freq);
    chan->setFrequency(freq * speed);
    chan->setVolume(volume);
    chan->setPan(balance);
    chan->setLoopCount(loopCount);

    FMOD_VECTOR pos = GameToFmodVec(soundPosition);
    FMOD_VECTOR vel = {0.f, 0.f, 0.f};
    chan->set3DAttributes(&pos, &vel);

    chan->setPaused(isPaused);

    return (SoundPlaybackID)chan;
}

void AudioSubsystem::SetSoundPosition(SoundPlaybackID soundPlaybackID, const Vec3& soundPosition)
{
    if (soundPlaybackID == MISSING_SOUND_ID) return;

    auto chan = (FMOD::Channel*)soundPlaybackID;

    bool isPlayingFlag = false;
    chan->isPlaying(&isPlayingFlag);
    if (!isPlayingFlag) return;

    FMOD_VECTOR pos = GameToFmodVec(soundPosition);
    FMOD_VECTOR vel = {0.f, 0.f, 0.f};
    chan->set3DAttributes(&pos, &vel);
}

bool AudioSubsystem::IsPlaying(SoundPlaybackID soundPlaybackID)
{
    if (soundPlaybackID == MISSING_SOUND_ID) return false;

    auto        chan          = (FMOD::Channel*)soundPlaybackID;
    bool        isPlayingFlag = false;
    FMOD_RESULT r             = chan->isPlaying(&isPlayingFlag);
    ValidateResult(r);
    return isPlayingFlag;
}

FMOD_VECTOR AudioSubsystem::GameToFmodVec(const Vec3& v)
{
    FMOD_VECTOR o;
    o.x = -v.y; // game.y → fmod.x (Right)
    o.y = v.z; // game.z → fmod.y (Up)
    o.z = -v.x; // game.x → fmod.z (Back)
    return o;
}

std::shared_ptr<enigma::resource::SoundResource> AudioSubsystem::LoadSound(const enigma::resource::ResourceLocation& location)
{
    if (!m_resourceSubsystem)
    {
        ERROR_RECOVERABLE("AudioSubsystem: No ResourceSubsystem set");
        return nullptr;
    }

    // Load directly through resource system (no caching in AudioSubsystem)
    auto resource = m_resourceSubsystem->GetResource(location);
    if (!resource)
    {
        ERROR_RECOVERABLE("AudioSubsystem: Failed to load sound resource: " + location.ToString());
        return nullptr;
    }

    // Cast to SoundResource
    auto soundResource = std::dynamic_pointer_cast<enigma::resource::SoundResource>(resource);
    if (!soundResource)
    {
        ERROR_RECOVERABLE("AudioSubsystem: Resource is not a SoundResource: " + location.ToString());
        return nullptr;
    }

    return soundResource;
}

SoundPlaybackID AudioSubsystem::PlaySound(const enigma::resource::ResourceLocation& location, bool isLooped, float volume, float balance, float speed, bool isPaused)
{
    auto soundResource = LoadSound(location);
    if (!soundResource)
    {
        return MISSING_SOUND_ID;
    }

    return soundResource->Play(*this, isLooped, volume, balance, speed, isPaused);
}

SoundPlaybackID AudioSubsystem::PlaySoundAt(const enigma::resource::ResourceLocation& location, const Vec3& position, bool isLooped, float volume, float balance, float speed, bool isPaused)
{
    auto soundResource = LoadSound(location);
    if (!soundResource)
    {
        return MISSING_SOUND_ID;
    }

    return soundResource->PlayAt(*this, position, isLooped, volume, balance, speed, isPaused);
}

#endif // !defined( ENGINE_DISABLE_AUDIO )
