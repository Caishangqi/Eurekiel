#include "Image.hpp"
#define STB_IMAGE_IMPLEMENTATION 
#include "EngineCommon.hpp"
#include "ThirdParty/stb/stb_image.h"

Image::Image()
{
}

Image::~Image()
{
}

Image::Image(const char* imageFilePath)
{
    IntVec2 dimensions    = IntVec2::ZERO; // This will be filled in for us to indicate image width & height
    int     bytesPerTexel = 0; // ...and how many color components the image had (e.g. 3=RGB=24bit, 4=RGBA=32bit)

    // Load (and decompress) the image RGB(A) bytes from a file on disk into a memory buffer (array of bytes)
    stbi_set_flip_vertically_on_load(1); // We prefer uvTexCoords has origin (0,0) at BOTTOM LEFT
    unsigned char* texelData = stbi_load(imageFilePath, &dimensions.x, &dimensions.y, &bytesPerTexel, 0);

    // Check if the load was successful
    GUARANTEE_OR_DIE(texelData, Stringf( "Failed to load image \"%s\"", imageFilePath ))
    m_imageFilePath = imageFilePath;
    m_dimensions    = dimensions;

    m_rgbaTexels.resize(m_dimensions.x * m_dimensions.y);

    for (int y = 0; y < m_dimensions.y; ++y)
    {
        for (int x = 0; x < m_dimensions.x; ++x)
        {
            int           index = (y * m_dimensions.x + x) * bytesPerTexel;
            unsigned char r     = texelData[index + 0];
            unsigned char g     = texelData[index + 1];
            unsigned char b     = texelData[index + 2];
            unsigned char a     = (bytesPerTexel == 4) ? texelData[index + 3] : 255;

            m_rgbaTexels[y * m_dimensions.x + x] = Rgba8(r, g, b, a);
        }
    }
    stbi_image_free(texelData);
}

Image::Image(IntVec2 size, Rgba8 color)
{
    m_dimensions = size;
    m_rgbaTexels.resize(m_dimensions.x * m_dimensions.y);
    std::fill(m_rgbaTexels.begin(), m_rgbaTexels.end(), color);
}

const std::string& Image::GetImageFilePath() const
{
    return m_imageFilePath;
}

IntVec2 Image::GetDimensions() const
{
    return m_dimensions;
}

const  void* Image::GetRawData() const 
{
    return m_rgbaTexels.data();
}

Rgba8 Image::GetTexelColor(const IntVec2& texelCoords) const
{
    return m_rgbaTexels[texelCoords.y * m_dimensions.x + texelCoords.x];
}

void Image::SetTexelColor(const IntVec2& texelCoords, const Rgba8& newColor)
{
    m_rgbaTexels[texelCoords.y * m_dimensions.x + texelCoords.x] = newColor;
}
