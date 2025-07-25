#pragma once
#include <vector>

#include "Rgba8.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/IntVec2.hpp"

struct Vertex_PCU;
class AABB2;
struct FloatRange;
struct Rgba8;

class HeatMaps
{
public:
    HeatMaps(const IntVec2& dimensions);

    IntVec2             GetDimensions() const;
    std::vector<float>& GetData();
    float               GetHighestHeatValueExcludingSpecialValue(float specialValue);
    void                SetAllValues(float value);
    float               GetValue(const IntVec2& tileCoords);
    void                SetValue(const IntVec2& tileCoords, float value);
    void                AddValue(const IntVec2& tileCoords, float value);
    bool                IsCoordsInBounds(const IntVec2& tileCoords);
    void                GeneratePath(std::vector<IntVec2>& outPath, IntVec2 startPos, IntVec2 endPos);
    /// This function can be used by game code to debug-draw a grid of colored tiles in an axis-aligned uniform tile
    /// grid, using shades of color to represent different heat map values within an expected range.
    ///
    /// For each tile, check if the value equals specialValue.  If so, push a quad of color specialColor.  Otherwise,
    /// use RangeMapClamped() to map the tile’s value from valueRange into [0,1] (clamped so no values are outside [0,1]),
    /// then use that to get an interpolated color between lowColor (at 0.0f) and highColor (at 1.0f).  Use the following
    /// standalone (non-member) utility/helper function to do this – write it if you haven’t written it already!
    /// 
    /// @param verts The new vertexes, six verts (two triangles) per tile, into the provided
    /// vertex array. So a TileHeatMap whose m_dimensions are (50,20) should add to the end of the vertex array 1000
    /// new quads = 2000 triangles = 6000 verts.
    /// @param totalBounds the area that the caller wishes to fill with the heat map’s 2D grid array of quads.
    /// So totalBounds.mins is the bottom-left corner of the first (bottom-left) tile’s quad, and totalBounds.maxs is
    /// the top-right corner of the last (top-right) tile’s quad.
    /// @param valueRange 
    /// @param lowColor 
    /// @param highColor 
    /// @param specialValue 
    /// @param specialColor 
    void AddVertsForDebugDraw(std::vector<Vertex_PCU>& verts, AABB2 totalBounds,
                              FloatRange               valueRange   = FloatRange(0.0f, 1.0f),
                              Rgba8                    lowColor     = Rgba8(0, 0, 0, 255), Rgba8 highColor    = Rgba8(255, 255, 255, 255),
                              float                    specialValue = 999.f, Rgba8               specialColor = Rgba8(67, 0, 141));
    void AddVertsForPathDebugDraw(std::vector<Vertex_PCU>& verts, const std::vector<IntVec2>& path, float perTileDrawSize = 1.0f, Rgba8          startColor = Rgba8::RED, Rgba8 endColor = Rgba8::GREEN,
                                  Rgba8                    pathColor                                                      = Rgba8::YELLOW, float opacity    = 0.5f);

private:
    IntVec2            m_dimensions;
    std::vector<float> m_values;

    int        GetIndexFromTileCoords(const IntVec2& tileCoords);
    IntVec2    GetTileCoordsFromIndex(int index);
    FloatRange GetRangeOfValuesExcludingSpecial(float specialValue);
};
