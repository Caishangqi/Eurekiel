#include "BitmapFont.hpp"
#include "SpriteDefinition.hpp"
#include "SpriteSheet.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/IntVec2.hpp"

BitmapFont::BitmapFont(const char* fontFilePathNameWithNoExtension, Texture& fontTexture)
{
    m_fontFilePathNameWithNoExtension = fontFilePathNameWithNoExtension;
    m_fontGlyphsSpriteSheet           = new SpriteSheet(fontTexture, IntVec2(16, 16));
}

Texture& BitmapFont::GetTexture()
{
    return m_fontGlyphsSpriteSheet->GetTexture();
}

void BitmapFont::AddVertsForText2D(std::vector<Vertex_PCU>& vertexArray, const Vec2& textMins, float cellHeight,
                                   const std::string&       text, const Rgba8&       tint, float     cellAspectScale)
{
    SpriteDefinition currentCharDefinition;
    AABB2            currentCharUVs; // Mapping UVs towards SpriteSheets
    AABB2            currentCharBounds; // Position that relative to whole string
    float            charWidth = cellHeight * cellAspectScale;

    for (size_t charIndex = 0; charIndex < text.length(); ++charIndex)
    {
        currentCharBounds.m_mins.x = textMins.x;
        currentCharBounds.m_mins.y = textMins.y;
        currentCharDefinition      = m_fontGlyphsSpriteSheet->GetSpriteDef(text[charIndex]);
        currentCharUVs             = currentCharDefinition.GetUVs();
        //  when go to next char, increase the width by char width
        currentCharBounds.m_mins = currentCharBounds.m_mins + Vec2(static_cast<float>(charIndex) * charWidth, 0);
        currentCharBounds.m_maxs = currentCharBounds.m_mins + Vec2(charWidth, cellHeight);
        AddVertsForAABB2D(vertexArray, currentCharBounds, tint, currentCharUVs.m_mins, currentCharUVs.m_maxs);
    }
}

/// By Shangqi Cai 2024/12/3
void BitmapFont::AddVertsForTextInBox2D(std::vector<Vertex_PCU>& vertexArray, const std::string& text, const AABB2& box, float cellHeight, const Rgba8& tint, float cellAspectScale,
                                        const Vec2&              alignment, TextDrawMode         mode, int          maxGlyphsToDraw)
{
    Strings            lineOfStrings = SplitStringOnDelimiter(text, '\n');
    std::vector<AABB2> lineBoxes;
    float              adjustedCellHeight = cellHeight;
    int                availableGlyphs    = maxGlyphsToDraw;
    // Check mode and modify cellHeight
    if (mode == SHRINK_TO_FIT)
    {
        float paragraphBoxLargestWidth = 0;
        float totalCellHeight          = cellHeight * static_cast<float>(lineOfStrings.size());
        if (totalCellHeight > box.GetDimensions().y)
            adjustedCellHeight = adjustedCellHeight * box.GetDimensions().y / totalCellHeight;
        for (int num = 0; num < static_cast<int>(lineOfStrings.size()); ++num)
        {
            std::string line         = lineOfStrings[num];
            float       textWidth    = GetTextWidth(cellHeight, line, cellAspectScale);
            paragraphBoxLargestWidth = std::max(textWidth, paragraphBoxLargestWidth);
        }
        if (paragraphBoxLargestWidth > box.GetDimensions().x)
            adjustedCellHeight = adjustedCellHeight * box.GetDimensions().x / paragraphBoxLargestWidth;
    }

    float paragraphBoxLargestWidth = 0;
    AABB2 paragraphBox;
    // First we create multiple line box and stack them together
    for (int num = 0; num < static_cast<int>(lineOfStrings.size()); ++num)
    {
        std::string line         = lineOfStrings[num];
        float       textWidth    = GetTextWidth(adjustedCellHeight, line, cellAspectScale);
        paragraphBoxLargestWidth = std::max(textWidth, paragraphBoxLargestWidth);
        AABB2 lineBox;
        lineBox.m_mins = Vec2::ZERO - Vec2(0, static_cast<float>(num + 1) * adjustedCellHeight);
        lineBox.m_maxs = lineBox.m_mins + Vec2(textWidth, adjustedCellHeight);
        lineBoxes.push_back(lineBox);
    }
    // We set the paragraphBox data
    paragraphBox.m_mins = Vec2::ZERO - Vec2(0, static_cast<float>(lineBoxes.size()) * adjustedCellHeight);
    paragraphBox.m_maxs = paragraphBox.m_mins + Vec2(paragraphBoxLargestWidth, static_cast<float>(lineBoxes.size()) * adjustedCellHeight);
    // Then we calculate the alignment of paragraphBox in outer box
    float alignmentX = (box.GetDimensions().x - paragraphBox.GetDimensions().x) * alignment.x;
    float alignmentY = (box.GetDimensions().y - paragraphBox.GetDimensions().y) * alignment.y;
    // We move paragraphBox to correspond alignment
    paragraphBox.SetCenter(Vec2(box.m_mins.x + alignmentX + paragraphBox.GetDimensions().x / 2, box.m_mins.y + alignmentY + paragraphBox.GetDimensions().y / 2));
    // We start calculate lineBoxes alignment and position correspond to paragraphBox
    for (int num = 0; num < static_cast<int>(lineBoxes.size()); ++num)
    {
        AABB2& lineBox           = lineBoxes[num];
        float  lineBoxAlignmentX = (paragraphBox.GetDimensions().x - lineBox.GetDimensions().x) * alignment.x;
        float  centerX           = paragraphBox.m_mins.x + lineBoxAlignmentX + lineBox.GetDimensions().x / 2;
        float  centerY           = paragraphBox.m_maxs.y - static_cast<float>(num) * lineBox.GetDimensions().y - lineBox.GetDimensions().y / 2;
        lineBox.SetCenter(Vec2(centerX, centerY));
        // Calculate the remain Glyphs
        availableGlyphs = availableGlyphs - static_cast<int>(lineOfStrings[num].length());
        if (availableGlyphs > 0)
        {
            // Put the text into the box
            AddVertsForText2D(vertexArray, lineBox.m_mins, adjustedCellHeight, lineOfStrings[num], tint, cellAspectScale);
        }
        else
        {
            std::string remainString = lineOfStrings[num].substr(0, lineOfStrings[num].length() + availableGlyphs); // cut the string based on remain
            AddVertsForText2D(vertexArray, lineBox.m_mins, adjustedCellHeight, remainString, tint, cellAspectScale);
            return;
        }
    }
}

void BitmapFont::AddVertsForText3DAtOriginXForward(
    std::vector<Vertex_PCU>& verts,
    float                    cellHeight,
    std::string const&       text,
    Rgba8 const&             tint,
    float                    cellAspect,
    Vec2 const&              alignment,
    int                      maxGlyphsToDraw)
{
    // Calculate total text width for given cellHeight, cellAspect, and the content of 'text'
    float textWidth = GetTextWidth(cellHeight, text, cellAspect);

    // Compute the initial Y position based on textWidth and alignment.x
    // Negative sign implies moving in the negative Y direction
    float currentY = -textWidth * alignment.x;

    // Compute Z boundaries (baseZ and boundZ). 
    // alignment.y controls how the text is positioned vertically.
    float baseZ  = -cellHeight * alignment.y;
    float boundZ = cellHeight * 0.5f;

    // Calculate the base glyph width (without per-glyph aspect scaling)
    float baseGlyphWidth = cellAspect * cellHeight;

    // Determine the number of glyphs that will actually be drawn
    // (cannot exceed either maxGlyphsToDraw or text.size())
    int glyphCount = (maxGlyphsToDraw > static_cast<int>(text.size()))
                         ? static_cast<int>(text.size())
                         : maxGlyphsToDraw;

    // Iterate over each glyph in the text
    for (int i = 0; i < glyphCount; ++i)
    {
        unsigned char glyph = text[i];

        // Skip newline for simplicity (no line-break logic here)
        if (glyph == '\n')
        {
            continue;
        }

        // Obtain per-glyph aspect ratio and sprite UVs from the font sprite sheet
        int   glyphIndex = static_cast<int>(glyph);
        float glyphRatio = GetGlyphAspect(glyphIndex);
        float glyphWidth = baseGlyphWidth * glyphRatio;

        // Build the quad for this glyph in 3D space (X=0, Y changes, Z in [baseZ, boundZ])
        AddVertsForQuad3D(
            verts,
            Vec3(0.f, currentY, baseZ),
            Vec3(0.f, currentY + glyphWidth, baseZ),
            Vec3(0.f, currentY + glyphWidth, boundZ),
            Vec3(0.f, currentY, boundZ),
            tint,
            m_fontGlyphsSpriteSheet->GetSpriteUVs(glyphIndex)
        );

        // Advance currentY by the width of this glyph
        currentY += glyphWidth;
    }
}


float BitmapFont::GetTextWidth(float cellHeight, const std::string& text, float cellAspectScale)
{
    int   numOfChars = static_cast<int>(text.length());
    float width      = cellHeight * cellAspectScale;
    return width * static_cast<float>(numOfChars);
}

BitmapFont::~BitmapFont()
{
    delete m_fontGlyphsSpriteSheet;
    m_fontGlyphsSpriteSheet = nullptr;
}

float BitmapFont::GetGlyphAspect(int glyphUnicode) const
{
    UNUSED(glyphUnicode)
    return m_fontDefaultAspect;
}
