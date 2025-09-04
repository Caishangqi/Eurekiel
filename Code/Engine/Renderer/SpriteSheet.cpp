#include "SpriteSheet.hpp"

#include "SpriteDefinition.hpp"
#include "Texture.hpp"
#include "Engine/Math/AABB2.hpp"

// The initialize list assign variable first
SpriteSheet::SpriteSheet(Texture& texture, const IntVec2& simpleGridLayout) : m_simpleGridLayout(simpleGridLayout),
                                                                              m_texture(texture)
{
    // Below is the second time variable assign
    IntVec2 dimension = texture.GetDimensions();
    // HSR
    for (int index = 0; index < simpleGridLayout.x * simpleGridLayout.y; index++)
    {
        int rowIndex = static_cast<int>(floor(index / simpleGridLayout.x));
        int colIndex = static_cast<int>(floor(index % simpleGridLayout.x));

        auto uvAtMins = Vec2(static_cast<float>(colIndex) / static_cast<float>(simpleGridLayout.x),
                             static_cast<float>(simpleGridLayout.y - 1 - rowIndex) / static_cast<float>(simpleGridLayout.y));
        auto uvAtMaxs = Vec2(static_cast<float>(colIndex + 1) / static_cast<float>(simpleGridLayout.x),
                             static_cast<float>(simpleGridLayout.y - rowIndex) / static_cast<float>(simpleGridLayout.y));
        m_spriteDefs.push_back(SpriteDefinition(this, index, uvAtMins, uvAtMaxs));
    }
}

Texture& SpriteSheet::GetTexture() const
{
    return m_texture;
}

int SpriteSheet::GetNumSprites() const
{
    return 0;
}

const SpriteDefinition& SpriteSheet::GetSpriteDef(int spriteIndex) const
{
    return m_spriteDefs[spriteIndex];
}

const SpriteDefinition& SpriteSheet::GetSpriteDef(IntVec2 spriteCoords) const
{
    return m_spriteDefs[spriteCoords.x + spriteCoords.y * m_simpleGridLayout.x];
}

void SpriteSheet::GetSpriteUVs(Vec2& out_uvAtMin, Vec2& out_uvAtMax, int spriteIndex) const
{
    // SpriteSheet sampling corrections
    out_uvAtMin = out_uvAtMin + Vec2(1 / 128.0f, 1 / 128.0f);
    out_uvAtMax = out_uvAtMax - Vec2(1 / 128.0f, 1 / 128.0f);
    /// We use 1/ 128.0f because the float can only preserve 6 digits after the decima
    /// the digits after 6th digit will be random number, so we want to ensure that the
    /// sampling is small enough but not smaller that float limitation
    m_spriteDefs[spriteIndex].GetUVs(out_uvAtMin, out_uvAtMax);
}

AABB2 SpriteSheet::GetSpriteUVs(int spriteIndex) const
{
    AABB2 uv;
    m_spriteDefs[spriteIndex].GetUVs(uv.m_mins, uv.m_maxs);
    return uv;
}

AABB2 SpriteSheet::GetSpriteUVs(IntVec2 spriteCoords) const
{
    AABB2 uv;
    m_spriteDefs[spriteCoords.x + spriteCoords.y * m_simpleGridLayout.x].GetUVs(uv.m_mins, uv.m_maxs);
    return uv;
}

IntVec2 SpriteSheet::GetSimpleGridSize() const
{
    return m_simpleGridLayout;
}

SpriteSheet& SpriteSheet::operator=(const SpriteSheet& spriteSheet)
{
    m_texture    = spriteSheet.GetTexture();
    m_spriteDefs = spriteSheet.m_spriteDefs;
    return *this;
}
