#include "HeatMaps.hpp"

#include <algorithm>

#include "Rgba8.hpp"
#include "VertexUtils.hpp"
#include "Engine/Math/AABB2.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/MathUtils.hpp"

HeatMaps::HeatMaps(const IntVec2& dimensions): m_dimensions(dimensions),
                                               m_values(dimensions.x * dimensions.y, 0.0f)
{
}

IntVec2 HeatMaps::GetDimensions() const
{
    return m_dimensions;
}

std::vector<float>& HeatMaps::GetData()
{
    return m_values;
}


float HeatMaps::GetHighestHeatValueExcludingSpecialValue(float specialValue)
{
    float highestValue = 0.0f;
    for (int index = 0; index < static_cast<int>(m_values.size()); index++)
    {
        if (!(m_values[index] >= specialValue))
        {
            highestValue = std::max(m_values[index], highestValue);
        }
    }
    return highestValue;
}

void HeatMaps::SetAllValues(float value)
{
    std::fill(m_values.begin(), m_values.end(), value);
}

float HeatMaps::GetValue(const IntVec2& tileCoords)
{
    return m_values[GetIndexFromTileCoords(tileCoords)];
}

void HeatMaps::SetValue(const IntVec2& tileCoords, float value)
{
    m_values[GetIndexFromTileCoords(tileCoords)] = value;
}

void HeatMaps::AddValue(const IntVec2& tileCoords, float value)
{
    m_values[GetIndexFromTileCoords(tileCoords)] += value;
}

bool HeatMaps::IsCoordsInBounds(const IntVec2& tileCoords)
{
    return tileCoords.x >= 0 &&
        tileCoords.y >= 0 &&
        tileCoords.x < m_dimensions.x &&
        tileCoords.y < m_dimensions.y;
}

void HeatMaps::GeneratePath(std::vector<IntVec2>& outPath, IntVec2 startPos, IntVec2 endPos)
{
    outPath.clear();
    IntVec2       currentTileCoords = endPos;
    const IntVec2 directions[4]     = {
        IntVec2(0, 1), // 上
        IntVec2(0, -1), // 下
        IntVec2(-1, 0), // 左
        IntVec2(1, 0) // 右
    };
    outPath.push_back(currentTileCoords);
    while (currentTileCoords != startPos)
    {
        float   minHeatValue   = GetValue(currentTileCoords);
        IntVec2 nextTileCoords = currentTileCoords;
        for (const IntVec2& direction : directions)
        {
            IntVec2 candidateCoords = currentTileCoords + direction;
            if (IsCoordsInBounds(candidateCoords))
            {
                float candidateValue = GetValue(candidateCoords);
                if (candidateValue < minHeatValue)
                {
                    minHeatValue   = candidateValue;
                    nextTileCoords = candidateCoords;
                }
            }
        }
        // 更新当前坐标并添加到路径
        currentTileCoords = nextTileCoords;
        outPath.push_back(currentTileCoords);
    }
}

void HeatMaps::AddVertsForDebugDraw(std::vector<Vertex_PCU>& verts, AABB2 totalBounds, FloatRange valueRange, Rgba8 lowColor, Rgba8 highColor, float specialValue, Rgba8 specialColor)
{
    Vec2  bottomLeft = totalBounds.m_mins;
    Vec2  topRight   = totalBounds.m_maxs;
    float unitX      = (topRight.x - bottomLeft.x) / static_cast<float>(m_dimensions.x);
    float unitY      = (topRight.y - bottomLeft.y) / static_cast<float>(m_dimensions.y);

    for (int index = 0; index < static_cast<int>(m_values.size()); index++)
    {
        AABB2   tileVertsBound;
        IntVec2 tileCoords    = GetTileCoordsFromIndex(index);
        tileVertsBound.m_mins = bottomLeft + Vec2(static_cast<float>(tileCoords.x) * unitX, static_cast<float>(tileCoords.y) * unitY);
        tileVertsBound.m_maxs = tileVertsBound.m_mins + Vec2(unitX, unitY);

        float value = m_values[index];
        Rgba8 color = lowColor;

        /// if (fabs(value - specialValue) < EPSILON)
        if (value == specialValue)
        {
            color = specialColor;
        }
        else
        {
            float range = RangeMapClamped(value, valueRange.m_min, valueRange.m_max, 0.0f, 1.0f);

            color = Interpolate(lowColor, highColor, range);
        }
        AddVertsForAABB2D(verts, tileVertsBound, color);
    }
}

void HeatMaps::AddVertsForPathDebugDraw(std::vector<Vertex_PCU>& verts, const std::vector<IntVec2>& path, float perTileDrawSize, Rgba8 startColor, Rgba8 endColor, Rgba8 pathColor, float opacity)
{
    if (path.size() == 0)
    {
        return;
    }
    IntVec2 end = path[0];

    endColor.a   = static_cast<unsigned char>(static_cast<float>(endColor.a) * opacity);
    startColor.a = static_cast<unsigned char>(static_cast<float>(startColor.a) * opacity);
    pathColor.a  = static_cast<unsigned char>(static_cast<float>(pathColor.a) * opacity);

    AddVertsForAABB2D(verts, AABB2(Vec2(end), Vec2(end) + Vec2(perTileDrawSize, perTileDrawSize)), endColor);
    IntVec2 start = path[static_cast<int>(path.size()) - 1];
    AddVertsForAABB2D(verts, AABB2(Vec2(start), Vec2(start) + Vec2(perTileDrawSize, perTileDrawSize)), startColor);
    if (path.size() > 2)
    {
        for (int index = 1; index < static_cast<int>(path.size()) - 1; index++)
        {
            IntVec2 pathTile = path[index];
            AddVertsForAABB2D(verts, AABB2(Vec2(pathTile), Vec2(pathTile) + Vec2(perTileDrawSize, perTileDrawSize)), pathColor);
        }
    }
}

int HeatMaps::GetIndexFromTileCoords(const IntVec2& tileCoords)
{
    return tileCoords.x + tileCoords.y * m_dimensions.x;
}

IntVec2 HeatMaps::GetTileCoordsFromIndex(int index)
{
    return IntVec2(index % m_dimensions.x, index / m_dimensions.x);
}

FloatRange HeatMaps::GetRangeOfValuesExcludingSpecial(float specialValue)
{
    FloatRange rangeOnNonSpecialValues(FLT_MAX, -FLT_MAX);
    int        numTiles = m_dimensions.x * m_dimensions.y;
    for (int index = 0; index < numTiles; index++)
    {
        float value = m_values[index];
        if (value != specialValue)
        {
            rangeOnNonSpecialValues.StretchToIncludeValue(value);
        }
    }
    return rangeOnNonSpecialValues;
}
