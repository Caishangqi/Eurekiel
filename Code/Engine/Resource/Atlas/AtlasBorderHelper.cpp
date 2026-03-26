#include "AtlasBorderHelper.hpp"

namespace enigma::resource
{
    void AtlasBorderHelper::ExtrudeBorders(
        Image&              atlasImage,
        const IntVec2&      spritePosition,
        const IntVec2&      spriteSize,
        int                 padding,
        BorderExtrusionMode mode)
    {
        if (mode == BorderExtrusionMode::NONE || padding <= 0)
            return;

        IntVec2 atlasDims = atlasImage.GetDimensions();

        // Helper lambda: safe SetTexelColor with bounds check
        auto safeSet = [&](int x, int y, Rgba8 color)
        {
            if (x >= 0 && x < atlasDims.x && y >= 0 && y < atlasDims.y)
                atlasImage.SetTexelColor(IntVec2(x, y), color);
        };

        int sx = spritePosition.x; // sprite left
        int sy = spritePosition.y; // sprite top
        int sw = spriteSize.x;     // sprite width
        int sh = spriteSize.y;     // sprite height

        // --- Top edge: replicate sprite top row upward ---
        for (int p = 1; p <= padding; ++p)
        {
            for (int x = 0; x < sw; ++x)
            {
                Rgba8 color = atlasImage.GetTexelColor(IntVec2(sx + x, sy));
                safeSet(sx + x, sy - p, color);
            }
        }

        // --- Bottom edge: replicate sprite bottom row downward ---
        for (int p = 1; p <= padding; ++p)
        {
            for (int x = 0; x < sw; ++x)
            {
                Rgba8 color = atlasImage.GetTexelColor(IntVec2(sx + x, sy + sh - 1));
                safeSet(sx + x, sy + sh - 1 + p, color);
            }
        }

        // --- Left edge: replicate sprite left column leftward ---
        for (int p = 1; p <= padding; ++p)
        {
            for (int y = 0; y < sh; ++y)
            {
                Rgba8 color = atlasImage.GetTexelColor(IntVec2(sx, sy + y));
                safeSet(sx - p, sy + y, color);
            }
        }

        // --- Right edge: replicate sprite right column rightward ---
        for (int p = 1; p <= padding; ++p)
        {
            for (int y = 0; y < sh; ++y)
            {
                Rgba8 color = atlasImage.GetTexelColor(IntVec2(sx + sw - 1, sy + y));
                safeSet(sx + sw - 1 + p, sy + y, color);
            }
        }

        // --- Four corners: fill with corner pixel ---
        Rgba8 topLeft     = atlasImage.GetTexelColor(IntVec2(sx, sy));
        Rgba8 topRight    = atlasImage.GetTexelColor(IntVec2(sx + sw - 1, sy));
        Rgba8 bottomLeft  = atlasImage.GetTexelColor(IntVec2(sx, sy + sh - 1));
        Rgba8 bottomRight = atlasImage.GetTexelColor(IntVec2(sx + sw - 1, sy + sh - 1));

        for (int py = 1; py <= padding; ++py)
        {
            for (int px = 1; px <= padding; ++px)
            {
                safeSet(sx - px, sy - py, topLeft);
                safeSet(sx + sw - 1 + px, sy - py, topRight);
                safeSet(sx - px, sy + sh - 1 + py, bottomLeft);
                safeSet(sx + sw - 1 + px, sy + sh - 1 + py, bottomRight);
            }
        }
    }
} // namespace enigma::resource