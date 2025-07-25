#include "SimpleTriangleFont.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/AABB2.hpp"

//------------------------------------------------------------------------------------------------
// A simple utility file for creating basic 5x9 pixel fonts out of pure triangles, i.e. does not
// require any external bitmap or TrueType fonts (or textures, or anything at all) in Data.
//


//------------------------------------------------------------------------------------------------
constexpr int TRITEXT_PIX_WIDE      = 5;
constexpr int TRITEXT_PIX_HIGH      = 9;
constexpr int TRITEXT_PIX_PER_GLYPH = TRITEXT_PIX_WIDE * TRITEXT_PIX_HIGH;
constexpr int TRITEXT_FIRST_ASCII   = 32;
constexpr int TRITEXT_LAST_ASCII    = 126;
constexpr int TRITEXT_NUM_ASCIIS    = 1 + TRITEXT_LAST_ASCII - TRITEXT_FIRST_ASCII;
constexpr int TRITEXT_NUM_ROWS      = TRITEXT_NUM_ASCIIS * TRITEXT_PIX_HIGH;
constexpr int TRITEXT_NUM_PIXELS    = TRITEXT_NUM_ASCIIS * TRITEXT_PIX_PER_GLYPH;

//------------------------------------------------------------------------------------------------
// 5x7 "pixel" data for each ASCII character from 32 (space) through 126 (~ tilde)
// Note: indexes are offset by -32 from ASCII value, so space(32) is at [0] at A(65) is at [33]
const char* g_triTextFontData[TRITEXT_NUM_ROWS] =
{
    //SPACE	  33 !     34 "     35 #     36 $     37 %     38 &     39 '     40 (     41 )     42 *     43 +     44 ,     45 -     46 .     47 /     48 '0'   49 '1'   50 '2'   51 '3'   52 '4'   53 '5'   54 '6'   55 '7'   56 '8'   57 '9'   58 :     59 ;     60 <     61 =     62 >     63 ?     64 @     65 A     66 B     67 C     68 D     69 E     70 F     71 G     72 H     73 I     74 J     75 K     76 L     77 M     78 N     79 O     80 P     81 Q     82 R     83 S     84 T     85 U     86 V     87 W     88 X     89 Y     90 Z     91 [     92 \     93 ]     94 ^     95 _     96 `     97 a     98 b     99 c     100 d    101 e    102 f    103 g    104 h    105 i    106 j    107 k    108 l    109 m    110 n    111 o    112 p    113 q    114 r    115 s    116 t    117 u    118 v    119 w    120 x    121 y    122 z    123 {    124 |    125 }    126 ~   
    ".....", "..O..", ".O.O.", ".O.O.", "..O..", "OO..O", ".OO..", "..O..", "...O.", ".O...", "..O..", ".....", ".....",
    ".....", ".....", ".....", ".OOO.", "..O..", ".OOO.", ".OOO.", "O..O.", "OOOOO", ".OOO.", "OOOOO", ".OOO.", ".OOO.",
    ".....", ".....", "...O.", ".....", ".O...", ".OOO.", ".OOO.", ".OOO.", "OOOO.", ".OOO.", "OOOO.", "OOOOO", "OOOOO",
    ".OOO.", "O...O", "OOOOO", ".OOOO", "O...O", "O....", "OO.OO", "O...O", ".OOO.", "OOOO.", ".OOO.", "OOOO.", ".OOO.",
    "OOOOO", "O...O", "O...O", "O...O", "O...O", "O...O", "OOOOO", ".OOO.", ".....", ".OOO.", "..O..", ".....", ".OO..",
    ".....", "O....", ".....", "....O", ".....", "..OOO", ".....", "O....", "..O..", "...O.", "O....", ".OO..", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".O...", ".....", ".....", ".....", ".....", ".....", ".....",
    "..OO.", "..O..", ".OO..", ".....",
    ".....", "..O..", ".O.O.", ".O.O.", ".OOOO", "OO..O", "O..O.", "..O..", "..O..", "..O..", "O.O.O", "..O..", ".....",
    ".....", ".....", "....O", "O...O", ".OO..", "O...O", "O...O", "O..O.", "O....", "O....", "....O", "O...O", "O...O",
    ".....", ".....", "..O..", ".....", "..O..", "O...O", "O...O", "O...O", "O...O", "O...O", "O...O", "O....", "O....",
    "O...O", "O...O", "..O..", "...O.", "O..O.", "O....", "O.O.O", "OO..O", "O...O", "O...O", "O...O", "O...O", "O...O",
    "..O..", "O...O", "O...O", "O...O", "O...O", "O...O", "....O", ".O...", "O....", "...O.", ".O.O.", ".....", "..OO.",
    ".....", "O....", ".....", "....O", ".....", ".O...", ".....", "O....", ".....", ".....", "O....", "..O..", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".O...", ".....", ".....", ".....", ".....", ".....", ".....",
    ".O...", "..O..", "...O.", ".OO.O",
    ".....", "..O..", ".....", "OOOOO", "O.O..", "...O.", "O.O..", ".....", ".O...", "...O.", ".OOO.", "..O..", ".....",
    ".....", ".....", "...O.", "O..OO", "..O..", "...O.", "....O", "O..O.", "OOOO.", "O....", "...O.", "O...O", "O...O",
    "..O..", "..O..", ".O...", "OOOOO", "...O.", "...O.", "O..OO", "O...O", "O...O", "O....", "O...O", "O....", "O....",
    "O....", "O...O", "..O..", "...O.", "O.O..", "O....", "O.O.O", "O.O.O", "O...O", "O...O", "O...O", "O...O", "O....",
    "..O..", "O...O", "O...O", "O...O", ".O.O.", ".O.O.", "...O.", ".O...", ".O...", "...O.", "O...O", ".....", "...O.",
    ".OOO.", "OOOO.", ".OOO.", ".OOOO", ".OOO.", "OOOO.", ".OOOO", "OOOO.", ".OO..", ".OOO.", "O...O", "..O..", "OOOO.",
    "O.OO.", ".OOO.", "OOOO.", ".OOOO", "O.OOO", ".OOOO", "OOOOO", "O...O", "O...O", "O...O", "O...O", "O...O", "OOOOO",
    ".O...", "..O..", "...O.", "O..O.",
    ".....", "..O..", ".....", ".O.O.", ".OOO.", "..O..", ".O...", ".....", ".O...", "...O.", "OOOOO", "OOOOO", ".....",
    "OOOOO", ".....", "..O..", "O.O.O", "..O..", "..O..", ".OOO.", "OOOOO", "....O", "OOOO.", "..O..", ".OOO.", ".OOOO",
    ".....", ".....", "O....", ".....", "....O", "..O..", "O.O.O", "OOOOO", "OOOO.", "O....", "O...O", "OOO..", "OOO..",
    "O.OOO", "OOOOO", "..O..", "...O.", "OO...", "O....", "O.O.O", "O..OO", "O...O", "OOOO.", "O...O", "OOOO.", ".OOO.",
    "..O..", "O...O", "O...O", "O.O.O", "..O..", "..O..", "..O..", ".O...", "..O..", "...O.", ".....", ".....", ".....",
    "....O", "O...O", "O...O", "O...O", "O...O", ".O...", "O...O", "O...O", "..O..", "...O.", "O..O.", "..O..", "O.O.O",
    "OO..O", "O...O", "O...O", "O...O", "OO...", "O....", ".O...", "O...O", "O...O", "O...O", ".O.O.", "O...O", "...O.",
    "O....", "..O..", "....O", ".....",
    ".....", "..O..", ".....", "OOOOO", "..O.O", ".O...", "O.O.O", ".....", ".O...", "...O.", ".OOO.", "..O..", ".....",
    ".....", ".....", ".O...", "OO..O", "..O..", ".O...", "....O", "...O.", "....O", "O...O", "..O..", "O...O", "....O",
    "..O..", "..O..", ".O...", "OOOOO", "...O.", "..O..", "O.OOO", "O...O", "O...O", "O....", "O...O", "O....", "O....",
    "O...O", "O...O", "..O..", "...O.", "O.O..", "O....", "O...O", "O...O", "O...O", "O....", "O.O.O", "O.O..", "....O",
    "..O..", "O...O", "O...O", "O.O.O", ".O.O.", "..O..", ".O...", ".O...", "...O.", "...O.", ".....", ".....", ".....",
    ".OOOO", "O...O", "O....", "O...O", "OOOOO", ".O...", "O...O", "O...O", "..O..", "...O.", "OOO..", "..O..", "O.O.O",
    "O...O", "O...O", "O...O", "O...O", "O....", ".OOO.", ".O...", "O...O", "O...O", "O.O.O", "..O..", ".O.O.", "..O..",
    ".O...", "..O..", "...O.", ".....",
    ".....", ".....", ".....", ".O.O.", "OOOO.", "O..OO", "O..O.", ".....", ".O...", "...O.", "O.O.O", "..O..", ".....",
    ".....", ".....", "O....", "O...O", "..O..", "O....", "O...O", "...O.", "O...O", "O...O", "..O..", "O...O", "....O",
    ".....", "..O..", "..O..", ".....", "..O..", ".....", "O....", "O...O", "O...O", "O...O", "O...O", "O....", "O....",
    "O...O", "O...O", "..O..", "O..O.", "O..O.", "O....", "O...O", "O...O", "O...O", "O....", "O..O.", "O..O.", "O...O",
    "..O..", "O...O", ".O.O.", "OO.OO", "O...O", "..O..", "O....", ".O...", "....O", "...O.", ".....", ".....", ".....",
    "O...O", "O...O", "O...O", "O...O", "O....", ".O...", "O...O", "O...O", "..O..", "...O.", "O..O.", "..O..", "O.O.O",
    "O...O", "O...O", "O...O", "O...O", "O....", "....O", ".O...", "O...O", ".O.O.", "O.O.O", ".O.O.", "..O..", ".O...",
    ".O...", "..O..", "...O.", ".....",
    ".....", "..O..", ".....", ".O.O.", "..O..", "O..OO", ".OO.O", ".....", "..O..", "..O..", "..O..", ".....", "..O..",
    ".....", ".....", ".....", ".OOO.", "OOOOO", "OOOOO", ".OOO.", "...O.", ".OOO.", ".OOO.", "..O..", ".OOO.", ".OOO.",
    ".....", ".O...", "...O.", ".....", ".O...", "..O..", ".OOOO", "O...O", "OOOO.", ".OOO.", "OOOO.", "OOOOO", "O....",
    ".OOO.", "O...O", "OOOOO", ".OO..", "O...O", "OOOOO", "O...O", "O...O", ".OOO.", "O....", ".OO.O", "O...O", ".OOO.",
    "..O..", ".OOOO", "..O..", "O...O", "O...O", "..O..", "OOOOO", ".OOO.", ".....", ".OOO.", ".....", ".....", ".....",
    ".OOOO", "OOOO.", ".OOO.", ".OOOO", ".OOOO", ".O...", ".OOOO", "O...O", ".OOO.", "...O.", "O...O", ".OOO.", "O.O.O",
    "O...O", ".OOO.", "OOOO.", ".OOOO", "O....", "OOOO.", "..OO.", ".OOOO", "..O..", "OO.OO", "O...O", ".O...", "OOOOO",
    "..OO.", "..O..", ".OO..", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", "...O.", ".O...", ".....", ".....", "..O..",
    ".....", "..O..", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", "....O", ".....", ".....", "O..O.", ".....", ".....", ".....",
    ".....", ".....", "O....", "....O", ".....", ".....", ".....", ".....", ".....", ".....", ".....", "O....", ".....",
    ".....", "..O..", ".....", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".O...",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", "OOOOO", ".....",
    ".....", ".....", ".....", ".....", ".....", ".....", "OOOO.", ".....", ".....", ".OO..", ".....", ".....", ".....",
    ".....", ".....", "O....", "....O", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....", ".....",
    ".....", ".....", ".....", ".....",
};


//------------------------------------------------------------------------------------------------
void SimpleTriangleFont_AddVertsForAABB2D(std::vector<Vertex_PCU>& mesh, const AABB2&    bounds, const Rgba8& tint,
                                          const Vec2&              uvAtMins, const Vec2& uvAtMaxs)
{
    Vec3 pos0(bounds.m_mins.x, bounds.m_mins.y, 0.f);
    Vec3 pos1(bounds.m_maxs.x, bounds.m_mins.y, 0.f);
    Vec3 pos2(bounds.m_maxs.x, bounds.m_maxs.y, 0.f);
    Vec3 pos3(bounds.m_mins.x, bounds.m_maxs.y, 0.f);

    mesh.emplace_back(pos0, tint, Vec2(uvAtMins.x, uvAtMins.y));
    mesh.emplace_back(pos1, tint, Vec2(uvAtMaxs.x, uvAtMins.y));
    mesh.emplace_back(pos2, tint, Vec2(uvAtMaxs.x, uvAtMaxs.y));

    mesh.emplace_back(pos0, tint, Vec2(uvAtMins.x, uvAtMins.y));
    mesh.emplace_back(pos2, tint, Vec2(uvAtMaxs.x, uvAtMaxs.y));
    mesh.emplace_back(pos3, tint, Vec2(uvAtMins.x, uvAtMaxs.y));
}


//------------------------------------------------------------------------------------------------
void AddVertsForGlyphTriangles2D(std::vector<Vertex_PCU>& verts, char             glyph, const Vec2& cellMins,
                                 const Vec2&              pixelSize, const Rgba8& color)
{
    if (glyph < TRITEXT_FIRST_ASCII || glyph > TRITEXT_LAST_ASCII)
        return;

    int triTextGlyphIndex = glyph - TRITEXT_FIRST_ASCII;
    for (int rowIndex = 0; rowIndex < TRITEXT_PIX_HIGH; ++rowIndex)
    {
        float       minY                 = cellMins.y + pixelSize.y * static_cast<float>(TRITEXT_PIX_HIGH - rowIndex - 1);
        float       maxY                 = minY + pixelSize.y;
        int         triTextFontDataIndex = triTextGlyphIndex + (rowIndex * TRITEXT_NUM_ASCIIS);
        const char* rowText              = g_triTextFontData[triTextFontDataIndex];

        for (int triTextCellIndex = 0; triTextCellIndex < TRITEXT_PIX_WIDE; ++triTextCellIndex)
        {
            if (rowText[triTextCellIndex] == '.')
                continue;

            float minX = cellMins.x + pixelSize.x * static_cast<float>(triTextCellIndex);
            float maxX = minX + pixelSize.x;

            SimpleTriangleFont_AddVertsForAABB2D(verts, AABB2(minX, minY, maxX, maxY), color, Vec2(0.f, 0.f),
                                                 Vec2(1.f, 1.f));
        }
    }
};


//------------------------------------------------------------------------------------------------
void AddVertsForTextTriangles2D(std::vector<Vertex_PCU>& verts, const std::string& text, const Vec2& startMins,
                                float                    cellHeight, const Rgba8&  color, float      cellAspect, bool isFlipped,
                                float                    spacingFraction)
{
    // #ToDo: Support flipped triangle fonts (e.g. when +Y is down)
    UNUSED(isFlipped);

    //	size_t estimatedNumberOfNewVerts = text.length() * TRITEXT_PIX_WIDE * TRITEXT_PIX_HIGH * (6 / 2); // assumes (liberally) that fewer than half of a glyph's pixels are lit on average
    //	verts.reserve( verts.size() + estimatedNumberOfNewVerts );

    float cellWidth    = cellHeight * cellAspect;
    float pixelWidth   = cellWidth * (1.f / static_cast<float>(TRITEXT_PIX_WIDE));
    float pixelHeight  = cellHeight * (1.f / static_cast<float>(TRITEXT_PIX_HIGH));
    float spacingWidth = cellWidth * spacingFraction;
    Vec2  cellSize(cellWidth, cellHeight);
    Vec2  pixelSize(pixelWidth, pixelHeight);
    Vec2  cellMins = startMins;

    int numGlyphs = static_cast<int>(text.length());
    for (int i = 0; i < numGlyphs; ++i)
    {
        char glyph = text[i];
        AddVertsForGlyphTriangles2D(verts, glyph, cellMins, pixelSize, color);
        cellMins.x += cellSize.x;
        cellMins.x += spacingWidth;
    }
}


//------------------------------------------------------------------------------------------------
float GetSimpleTriangleStringWidth(const std::string& text, float cellHeight, float cellAspect, float spacingFraction)
{
    float cellWidth = cellHeight * cellAspect;
    float gapWidth  = cellWidth * spacingFraction;
    float numCells  = static_cast<float>(text.length());
    float numGaps   = numCells - 1.f;
    if (numGaps < 0.f)
        numGaps = 0.f;

    float totalWidth = (numCells * cellWidth) + (numGaps * gapWidth);
    return totalWidth;
}
