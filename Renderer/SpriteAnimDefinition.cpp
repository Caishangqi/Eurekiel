#include "SpriteAnimDefinition.hpp"

#include "SpriteSheet.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

SpriteAnimDefinition::SpriteAnimDefinition(const SpriteSheet& sheet, int startSpriteIndex, int endSpriteIndex, float framesPerSecond, SpriteAnimPlaybackType playbackType): m_spriteSheet(sheet),
    m_startSpriteIndex(startSpriteIndex), m_endSpriteIndex(endSpriteIndex), m_framesPerSecond(framesPerSecond), m_playbackType(playbackType)
{
}

const SpriteDefinition& SpriteAnimDefinition::GetSpriteDefAtTime(float seconds) const
{
    int totalFrames = m_endSpriteIndex - m_startSpriteIndex + 1;
    if (totalFrames <= 0)
    {
        ERROR_AND_DIE("Invalid sprite indices for animation")
    }

    float totalSecondsForOneLoop = static_cast<float>(totalFrames) / m_framesPerSecond;
    float currentTimeInLoop      = seconds;

    switch (m_playbackType)
    {
    case SpriteAnimPlaybackType::ONCE:
        if (seconds >= totalSecondsForOneLoop)
        {
            return m_spriteSheet.GetSpriteDef(m_endSpriteIndex);
        }
        break;
    case SpriteAnimPlaybackType::LOOP:
        currentTimeInLoop = fmod(seconds, totalSecondsForOneLoop);
        break;
    case SpriteAnimPlaybackType::PING_PONG:
        {
            float pingPongTime = fmod(seconds, 2.0f * totalSecondsForOneLoop);
            if (pingPongTime > totalSecondsForOneLoop)
            {
                pingPongTime = 2.0f * totalSecondsForOneLoop - pingPongTime;
            }
            currentTimeInLoop = pingPongTime;
            break;
        }
    default:
        ERROR_AND_DIE("Unknown playback type")
    }

    int currentFrameIndex = static_cast<int>(currentTimeInLoop * m_framesPerSecond) % totalFrames;
    int spriteIndex       = m_startSpriteIndex + currentFrameIndex;

    return m_spriteSheet.GetSpriteDef(spriteIndex);
}

float SpriteAnimDefinition::GetDuration() const
{
    int totalFrames = m_endSpriteIndex - m_startSpriteIndex + 1;
    return static_cast<float>(totalFrames) / m_framesPerSecond;
}

int SpriteAnimDefinition::GetTotalFrameInCycle()
{
    return m_endSpriteIndex - m_startSpriteIndex + 1;
}
