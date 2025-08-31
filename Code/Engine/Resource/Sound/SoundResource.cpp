#include "SoundResource.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/Vec3.hpp"

using namespace enigma::resource;

#if !defined(ENGINE_DISABLE_AUDIO)

SoundResource::SoundResource(const ResourceMetadata& metadata, FMOD::Sound* fmodSound, const SoundConfig& config)
    : m_metadata(metadata), m_fmodSound(fmodSound), m_config(config)
{
    if (m_fmodSound)
    {
        // Configure FMOD sound properties based on config
        if (m_config.is3D)
        {
            m_fmodSound->set3DMinMaxDistance(m_config.minDistance, m_config.maxDistance);
        }

        // Set default mode flags
        FMOD_MODE mode = FMOD_DEFAULT;
        if (m_config.loop)
            mode |= FMOD_LOOP_NORMAL;
        if (m_config.is3D)
            mode |= FMOD_3D;
        else
            mode |= FMOD_2D;

        m_fmodSound->setMode(mode);
    }
}

SoundResource::~SoundResource()
{
    Unload();
}

const void* SoundResource::GetRawData() const
{
    if (!m_rawDataCached)
    {
        CacheRawData();
    }
    return m_rawData.data();
}

size_t SoundResource::GetRawDataSize() const
{
    if (!m_rawDataCached)
    {
        CacheRawData();
    }
    return m_rawData.size();
}

SoundPlaybackID SoundResource::Play(AudioSubsystem& audioSystem, bool isLooped, float volume, float balance, float speed, bool isPaused) const
{
    UNUSED(audioSystem)
    if (!IsLoaded())
        return MISSING_SOUND_ID;

    // Apply config defaults
    volume *= m_config.volume;
    speed *= m_config.pitch;
    if (m_config.loop && !isLooped)
        isLooped = true;

    // Use FMOD sound directly for playback
    FMOD::Channel* channel = nullptr;
    FMOD::System*  system  = nullptr;

    if (m_fmodSound)
    {
        m_fmodSound->getSystemObject(&system);
        if (system)
        {
            system->playSound(m_fmodSound, nullptr, isPaused, &channel);

            if (channel)
            {
                // Configure channel properties
                int          loopCount    = isLooped ? -1 : 0;
                unsigned int playbackMode = isLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;

                if (m_config.is3D)
                    playbackMode |= FMOD_3D;
                else
                    playbackMode |= FMOD_2D;

                channel->setMode(playbackMode);

                float frequency;
                channel->getFrequency(&frequency);
                channel->setFrequency(frequency * speed);
                channel->setVolume(volume);
                channel->setPan(balance);
                channel->setLoopCount(loopCount);
            }
        }
    }

    return (SoundPlaybackID)channel;
}

SoundPlaybackID SoundResource::PlayAt(AudioSubsystem& audioSystem, const Vec3& position, bool isLooped, float volume, float balance, float speed, bool isPaused) const
{
    UNUSED(audioSystem)
    if (!IsLoaded() || !m_config.is3D)
        return MISSING_SOUND_ID;

    // Apply config defaults
    volume *= m_config.volume;
    speed *= m_config.pitch;
    if (m_config.loop && !isLooped)
        isLooped = true;

    FMOD::Channel* channel = nullptr;
    FMOD::System*  system  = nullptr;

    if (m_fmodSound)
    {
        m_fmodSound->getSystemObject(&system);
        if (system)
        {
            system->playSound(m_fmodSound, nullptr, true, &channel); // Start paused

            if (channel)
            {
                // Configure channel properties
                int          loopCount    = isLooped ? -1 : 0;
                unsigned int playbackMode = isLooped ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
                playbackMode |= FMOD_3D;

                channel->setMode(playbackMode);

                float frequency;
                channel->getFrequency(&frequency);
                channel->setFrequency(frequency * speed);
                channel->setVolume(volume);
                channel->setPan(balance);
                channel->setLoopCount(loopCount);

                // Set 3D position
                FMOD_VECTOR pos = {-position.y, position.z, -position.x}; // Convert to FMOD coordinate system
                FMOD_VECTOR vel = {0.0f, 0.0f, 0.0f};
                channel->set3DAttributes(&pos, &vel);

                channel->setPaused(isPaused);
            }
        }
    }

    return (SoundPlaybackID)channel;
}

float SoundResource::GetLength() const
{
    if (!IsLoaded())
        return 0.0f;

    unsigned int length = 0;
    FMOD_RESULT  result = m_fmodSound->getLength(&length, FMOD_TIMEUNIT_MS);
    if (result == FMOD_OK)
    {
        return length / 1000.0f; // Convert to seconds
    }

    return 0.0f;
}

int SoundResource::GetChannels() const
{
    if (!IsLoaded())
        return 0;

    int         channels = 0;
    FMOD_RESULT result   = m_fmodSound->getFormat(nullptr, nullptr, &channels, nullptr);
    if (result == FMOD_OK)
    {
        return channels;
    }

    return 0;
}

int SoundResource::GetFrequency() const
{
    if (!IsLoaded())
        return 0;

    float       frequency = 0.0f;
    int         priority  = 0;
    FMOD_RESULT result    = m_fmodSound->getDefaults(&frequency, &priority);
    if (result == FMOD_OK)
    {
        return static_cast<int>(frequency);
    }

    return 0;
}

FMOD_SOUND_FORMAT SoundResource::GetFormat() const
{
    if (!IsLoaded())
        return FMOD_SOUND_FORMAT_NONE;

    FMOD_SOUND_FORMAT format = FMOD_SOUND_FORMAT_NONE;
    FMOD_RESULT       result = m_fmodSound->getFormat(nullptr, &format, nullptr, nullptr);
    if (result == FMOD_OK)
    {
        return format;
    }

    return FMOD_SOUND_FORMAT_NONE;
}

void SoundResource::Unload()
{
    if (m_fmodSound)
    {
        // Only attempt to release if FMOD system might still be valid
        // This prevents crashes during shutdown when FMOD system is already released
        FMOD_RESULT result = m_fmodSound->release();
        if (result != FMOD_OK)
        {
            // Don't treat release failures during shutdown as fatal errors
            // The FMOD system might already be shutdown
        }
        m_fmodSound = nullptr;
    }

    m_rawData.clear();
    m_rawDataCached = false;
}

bool SoundResource::Reload(const std::vector<uint8_t>& data, const SoundConfig& config)
{
    UNUSED(data)

    // Unload current sound
    Unload();

    // Update config
    m_config = config;

    // Create new FMOD sound from data
    FMOD::System* system = nullptr;

    UNUSED(system)

    // We need access to the FMOD system, but we don't have it directly
    // This would need to be passed in or accessed through a global AudioSubsystem instance
    // For now, we'll return false as this operation requires more context

    ERROR_RECOVERABLE("SoundResource::Reload not fully implemented - requires AudioSubsystem context");
    return false;
}

void SoundResource::CacheRawData() const
{
    if (!IsLoaded() || m_rawDataCached)
        return;

    // Get sound information
    FMOD_SOUND_TYPE   soundType;
    FMOD_SOUND_FORMAT format;
    int               channels, bits;

    FMOD_RESULT result = m_fmodSound->getFormat(&soundType, &format, &channels, &bits);
    if (result != FMOD_OK)
        return;

    unsigned int length;
    result = m_fmodSound->getLength(&length, FMOD_TIMEUNIT_PCMBYTES);
    if (result != FMOD_OK)
        return;

    // Lock the sound data
    void *       ptr1, *ptr2;
    unsigned int len1,  len2;

    result = m_fmodSound->lock(0, length, &ptr1, &ptr2, &len1, &len2);
    if (result == FMOD_OK)
    {
        m_rawData.clear();
        m_rawData.reserve(len1 + len2);

        if (ptr1 && len1 > 0)
        {
            const uint8_t* data1 = static_cast<const uint8_t*>(ptr1);
            m_rawData.insert(m_rawData.end(), data1, data1 + len1);
        }

        if (ptr2 && len2 > 0)
        {
            const uint8_t* data2 = static_cast<const uint8_t*>(ptr2);
            m_rawData.insert(m_rawData.end(), data2, data2 + len2);
        }

        m_fmodSound->unlock(ptr1, ptr2, len1, len2);
        m_rawDataCached = true;
    }
}

#else // ENGINE_DISABLE_AUDIO

// Dummy implementations when audio is disabled
SoundResource::SoundResource(const ResourceMetadata& metadata, FMOD::Sound* fmodSound, const SoundConfig& config)
    : m_metadata(metadata), m_fmodSound(nullptr), m_config(config)
{
    UNUSED(fmodSound);
}

SoundResource::~SoundResource()
{
}

const void* SoundResource::GetRawData() const { return nullptr; }
size_t      SoundResource::GetRawDataSize() const { return 0; }

SoundPlaybackID SoundResource::Play(AudioSubsystem& audioSystem, bool isLooped, float volume, float balance, float speed, bool isPaused) const
{
    UNUSED(audioSystem);
    UNUSED(isLooped);
    UNUSED(volume);
    UNUSED(balance);
    UNUSED(speed);
    UNUSED(isPaused);
    return MISSING_SOUND_ID;
}

SoundPlaybackID SoundResource::PlayAt(AudioSubsystem& audioSystem, const Vec3& position, bool isLooped, float volume, float balance, float speed, bool isPaused) const
{
    UNUSED(audioSystem);
    UNUSED(position);
    UNUSED(isLooped);
    UNUSED(volume);
    UNUSED(balance);
    UNUSED(speed);
    UNUSED(isPaused);
    return MISSING_SOUND_ID;
}

float             SoundResource::GetLength() const { return 0.0f; }
int               SoundResource::GetChannels() const { return 0; }
int               SoundResource::GetFrequency() const { return 0; }
FMOD_SOUND_FORMAT SoundResource::GetFormat() const { return FMOD_SOUND_FORMAT_NONE; }
void              SoundResource::Unload()
{
}
bool SoundResource::Reload(const std::vector<uint8_t>& data, const SoundConfig& config)
{
    UNUSED(data);
    UNUSED(config);
    return false;
}
void SoundResource::CacheRawData() const
{
}

#endif // !defined(ENGINE_DISABLE_AUDIO)
