#include "SpriteDefinition.hpp"

#include "SpriteSheet.hpp"
#include "Texture.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Math/AABB2.hpp"


SpriteDefinition::SpriteDefinition(const SpriteSheet* spriteSheet, int spriteIndex, const Vec2& uvAtMin,
                                   const Vec2&        uvAtMax) : m_spriteSheet(spriteSheet), m_spriteIndex(spriteIndex),
                                                          m_uvAtMins(uvAtMin), m_uvAtMaxs(uvAtMax)
{
}

SpriteDefinition::SpriteDefinition() : m_spriteSheet(nullptr), m_spriteIndex(0),
                                       m_uvAtMins(Vec2::ZERO), m_uvAtMaxs(Vec2::ONE)
{
}


void SpriteDefinition::GetUVs(Vec2& out_uvAtMin, Vec2& out_uvAtMax) const
{
    out_uvAtMin = m_uvAtMins;
    out_uvAtMax = m_uvAtMaxs;
}

AABB2 SpriteDefinition::GetUVs() const
{
    return AABB2(m_uvAtMins, m_uvAtMaxs);
}

const SpriteSheet& SpriteDefinition::GetSpriteSheet() const
{
    return *m_spriteSheet;
}

Texture& SpriteDefinition::GetTexture() const
{
    return m_spriteSheet->m_texture;
}

float SpriteDefinition::GetAspect() const
{
    IntVec2 dimension = m_spriteSheet->m_texture.GetDimensions();
    float   unitU     = m_uvAtMaxs.x - m_uvAtMins.x;
    float   unitV     = m_uvAtMaxs.y - m_uvAtMins.y;
    float   unitW     = static_cast<float>(dimension.x) * unitU;
    float   unitH     = static_cast<float>(dimension.y) * unitV;
    return (unitW / unitH);
}

SpriteDefinition& SpriteDefinition::operator=(const SpriteDefinition& other)
{
    if (&other == this)
    {
        return *this;
    }
    m_spriteIndex = other.m_spriteIndex;
    m_uvAtMins    = other.m_uvAtMins;
    m_uvAtMaxs    = other.m_uvAtMaxs;
    m_spriteSheet = other.m_spriteSheet;
    return *this;
}
