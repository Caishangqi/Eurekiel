#pragma once
#include <string>
#include <vector>

#include "Rgba8.hpp"
#include "Engine/Math/IntVec2.hpp"

class Image
{
public:
    Image();
    ~Image();
    Image(const char* imageFilePath);
    Image(IntVec2 size, Rgba8 color);

    const std::string& GetImageFilePath() const;
    IntVec2            GetDimensions() const;
    const void*        GetRawData();

    Rgba8 GetTexelColor(const IntVec2& texelCoords) const;
    void  SetTexelColor(const IntVec2& texelCoords, const Rgba8& newColor);

private:
    std::string        m_imageFilePath;
    IntVec2            m_dimensions = IntVec2(0, 0);
    std::vector<Rgba8> m_rgbaTexels; // or Rgba8* m_rgbaTexels = nullptr; if you prefer new[] and delete[]
};
