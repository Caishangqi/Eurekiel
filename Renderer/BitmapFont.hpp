#pragma once
#include <string>
#include <vector>

#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"

class AABB2;
struct Vertex_PCU;
class Texture;
class SpriteSheet;

enum TextDrawMode
{
    ///Text blocks which are wider, or taller, than the box will be uniformly scaled down
    ///by the precise amount required to fit within the box.
    SHRINK_TO_FIT,
    ///Position the text based on its alignment; if the text block is too large (vertically
    ///and/or horizontally), it merely overruns the bounds of the box.  Note: for left-aligned
    ///text, horizontal overrun is off the right edge of the box; for right-aligned, overrun
    ///is off the left edge; for centered, overrun is off both edges.
    OVERRUN
};

class BitmapFont

{
    friend class Renderer; // Only the Renderer can create new BitmapFont objects!
    friend class IRenderer;
    friend class DX12Renderer;
    friend class DX11Renderer;

    BitmapFont(const char* fontFilePathNameWithNoExtension, Texture& fontTexture);

public:
    Texture& GetTexture();
    ///
    /// Example Usage AddVertsForText2D( textVerts, Vec2( 100.f, 200.f ), 30.f, "Hello, world" );
    /// 
    /// Example Usage AddVertsForText2D( textVerts, Vec2( 250.f, 400.f ), 15.f, "It's nice to have options!", Rgba8::RED, 0.6f );
    /// 
    /// @param vertexArray the vertices that will received
    /// @param textMins The text left bottom position
    /// @param cellHeight The text height
    /// @param text The target text you want to render
    /// @param tint Color that tint to the text
    /// @param cellAspectScale The cell aspect scale that will apply to cell height to calculate width
    void AddVertsForText2D(std::vector<Vertex_PCU>& vertexArray, const Vec2&       textMins,
                           float                    cellHeight, const std::string& text, const Rgba8& tint = Rgba8::WHITE,
                           float                    cellAspectScale                                        = 1.f);
    /// Allows creating vertexes (triangles) to draw a specified text string within a specified AABB2.\n
    /// Supports 2D alignment as Vector2 (0,0 means bottom-left, .5,.5 centered, 1,1 top-right).\n
    /// Supports newline ('\\n’) m_characters, which break text into separate lines.\n
    /// Takes a TextDrawMode enum which (for now) offers at least two modes\n
    /// Takes a maxGlyphsToDraw argument (default=999999999, or if you prefer, INT_MAX)\n
    /// @param vertexArray 
    /// @param text 
    /// @param box 
    /// @param cellHeight 
    /// @param tint 
    /// @param cellAspectScale 
    /// @param alignment 
    /// @param mode 
    /// @param maxGlyphsToDraw 
    void AddVertsForTextInBox2D(std::vector<Vertex_PCU>& vertexArray, const std::string& text, const AABB2& box, float cellHeight, const Rgba8& tint = Rgba8::WHITE,
                                float                    cellAspectScale = 1.f, const Vec2& alignment = Vec2(.5f, .5f), TextDrawMode mode = SHRINK_TO_FIT, int maxGlyphsToDraw = 99999999);
    /// @brief  Converts each character in the given string into 3D vertex data appended to `verts`,
    ///         with the X-axis pointing forward (towards the screen or the camera), the Y-axis aligned horizontally,
    ///         and the Z-axis aligned vertically.
    ///
    /// @param [out] verts            A container to which the resulting vertices will be appended.
    /// @param        cellHeight       The height of each character in 3D space, controlling the overall text size.
    /// @param        text             The string content to be rendered.
    /// @param        tint             Vertex color, defaulting to Rgba8::WHITE.
    /// @param        cellAspect       The aspect ratio of each character relative to its height. The default of 1.0f means
    ///                                equal width and height.
    /// @param        alignment        The alignment factors for the text in the Y and Z directions, typically in the range [0, 1].
    ///                                For example, (0.5f, 0.5f) means centering the text along both axes.
    /// @param        maxGlyphsToDraw  The maximum number of m_characters to draw. Any excess m_characters are truncated; defaults to 999.
    ///
    /// During execution:
    /// - The function determines each character's size based on the given `cellHeight` and `cellAspect`.
    /// - The `alignment` parameter offsets the starting position of the text for centering or other alignment forms.
    /// - Each character's UV coordinates are computed internally, and then converted to a quadrilateral using `AddVertsForQuad3D`.
    /// - The X direction is considered “forward,” so all vertices are at X=0 by default, with Y and Z coordinates
    ///   positioning the text horizontally and vertically.
    /// - Newline m_characters (`\n`) are skipped, and no line-breaking logic is implemented.
    ///
    /// @see     AddVertsForQuad3D
    /// @see     Rgba8
    /// @see     Vec2
    void AddVertsForText3DAtOriginXForward(
        std::vector<Vertex_PCU>& verts,
        float                    cellHeight,
        const std::string&       text,
        const Rgba8&             tint            = Rgba8::WHITE,
        float                    cellAspect      = 1.0f,
        const Vec2&              alignment       = Vec2(0.5f, 0.5f),
        int                      maxGlyphsToDraw = 999
    );


    /// 
    /// @param cellHeight The text height
    /// @param text The text content
    /// @param cellAspectScale The scale rate
    /// @return The width of the target text
    float GetTextWidth(float cellHeight, const std::string& text, float cellAspectScale = 1.f);

    ~BitmapFont();

protected:
    float GetGlyphAspect(int glyphUnicode) const; // For now this will always return m_fontDefaultAspect

    std::string  m_fontFilePathNameWithNoExtension;
    SpriteSheet* m_fontGlyphsSpriteSheet;
    float        m_fontDefaultAspect = 1.0f; // For basic (tier 1) fonts, set this to the aspect of the sprite sheet texture
};
