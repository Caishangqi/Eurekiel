#pragma once
class SpriteDefinition;
class SpriteSheet;

enum class SpriteAnimPlaybackType
{
    ONCE, // for 5-frame anim, plays 0,1,2,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4...
    LOOP, // for 5-frame anim, plays 0,1,2,3,4,0,1,2,3,4,0,1,2,3,4,0,1,2,3,4,0,1,2,3,4,0...
    PING_PONG, // for 5-frame anim, plays 0,1,2,3,4,3,2,1,0,1,2,3,4,3,2,1,0,1,2,3,4,3,2,1,0,1...
};

class SpriteAnimDefinition
{
public:
    SpriteAnimDefinition(const SpriteSheet& sheet, int                              startSpriteIndex, int endSpriteIndex,
                         float              framesPerSecond, SpriteAnimPlaybackType playbackType = SpriteAnimPlaybackType::LOOP);
    const SpriteDefinition& GetSpriteDefAtTime(float seconds) const; // Most of the logic for this class is done here!
    float                   GetDuration() const;
    int                     GetTotalFrameInCycle();

private:
    const SpriteSheet&     m_spriteSheet;
    int                    m_startSpriteIndex = -1;
    int                    m_endSpriteIndex   = -1;
    float                  m_framesPerSecond  = 1.f;
    SpriteAnimPlaybackType m_playbackType     = SpriteAnimPlaybackType::LOOP;
};
