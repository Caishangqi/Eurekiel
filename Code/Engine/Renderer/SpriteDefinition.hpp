#pragma once
#include "Engine/Math/Vec2.hpp"

class Texture;
class AABB2;
class SpriteSheet;

class SpriteDefinition
{
public:
    explicit SpriteDefinition(const SpriteSheet* spriteSheet, int spriteIndex, const Vec2& uvAtMin,
                              const Vec2&        uvAtMax);
    SpriteDefinition();

    void               GetUVs(Vec2& out_uvAtMin, Vec2& out_uvAtMax) const;
    AABB2              GetUVs() const;
    const SpriteSheet& GetSpriteSheet() const;
    Texture&           GetTexture() const;
    float              GetAspect() const;

    SpriteDefinition& operator=(const SpriteDefinition&);

protected:
    const SpriteSheet* m_spriteSheet;
    int                m_spriteIndex = -1;
    Vec2               m_uvAtMins    = Vec2::ZERO;
    Vec2               m_uvAtMaxs    = Vec2::ONE;
};
