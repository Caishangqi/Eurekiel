#pragma once
#include <vector>
#include "SpriteDefinition.hpp"
#include "Engine/Math/IntVec2.hpp"

class AABB2;
struct Vec2;
struct IntVec2;
class Texture;

class SpriteSheet
{
public:
    friend class SpriteDefinition;
    explicit SpriteSheet(Texture& texture, const IntVec2& simpleGridLayout);

    Texture&                GetTexture() const;
    int                     GetNumSprites() const;
    const SpriteDefinition& GetSpriteDef(int spriteIndex) const;
    const SpriteDefinition& GetSpriteDef(IntVec2 spriteCoords) const;
    void                    GetSpriteUVs(Vec2& out_uvAtMin, Vec2& out_uvAtMax, int spriteIndex) const;
    AABB2                   GetSpriteUVs(int spriteIndex) const;
    AABB2                   GetSpriteUVs(IntVec2 spriteCoords) const;
    IntVec2                 GetSimpleGridSize() const;

    SpriteSheet& operator=(const SpriteSheet& spriteSheet);

protected:
    IntVec2                       m_simpleGridLayout;
    Texture&                      m_texture; // reference members must be set in constructor's initializer list
    std::vector<SpriteDefinition> m_spriteDefs;
};
