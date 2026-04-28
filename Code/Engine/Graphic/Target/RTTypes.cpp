#include "Engine/Graphic/Target/RTTypes.hpp"

namespace enigma::graphic
{
    ClearValue ClearValue::Color(const Rgba8& rgba8)
    {
        ClearValue cv;
        cv.colorRgba8 = rgba8;
        return cv;
    }

    ClearValue ClearValue::Color(float r, float g, float b, float a)
    {
        unsigned char rByte = static_cast<unsigned char>(r * 255.0f);
        unsigned char gByte = static_cast<unsigned char>(g * 255.0f);
        unsigned char bByte = static_cast<unsigned char>(b * 255.0f);
        unsigned char aByte = static_cast<unsigned char>(a * 255.0f);

        ClearValue cv;
        cv.colorRgba8 = Rgba8(rByte, gByte, bByte, aByte);
        return cv;
    }

    ClearValue ClearValue::Depth(float depth, uint8_t stencil)
    {
        ClearValue cv;
        cv.depthStencil.depth   = depth;
        cv.depthStencil.stencil = stencil;
        return cv;
    }

    void ClearValue::GetColorAsFloats(float* outFloats) const
    {
        colorRgba8.GetAsFloats(outFloats);
    }

    RenderTargetConfig RenderTargetConfig::ColorTarget(
        const std::string& name,
        int                width, int height,
        DXGI_FORMAT        format,
        bool               enableFlipper,
        LoadAction         loadAction,
        ClearValue         clearValue,
        bool               enableMipmap,
        bool               allowLinearFilter,
        int                sampleCount)
    {
        RenderTargetConfig config;
        config.name              = name;
        config.width             = width;
        config.height            = height;
        config.format            = format;
        config.enableFlipper     = enableFlipper;
        config.loadAction        = loadAction;
        config.clearValue        = clearValue;
        config.enableMipmap      = enableMipmap;
        config.allowLinearFilter = allowLinearFilter;
        config.sampleCount       = sampleCount;
        return config;
    }

    RenderTargetConfig RenderTargetConfig::DepthTarget(
        const std::string& name,
        int                width, int height,
        DXGI_FORMAT        format,
        bool               enableFlipper,
        LoadAction         loadAction,
        ClearValue         clearValue,
        bool               enableMipmap,
        bool               allowLinearFilter,
        int                sampleCount)
    {
        RenderTargetConfig config;
        config.name              = name;
        config.width             = width;
        config.height            = height;
        config.format            = format;
        config.enableFlipper     = enableFlipper;
        config.loadAction        = loadAction;
        config.clearValue        = clearValue;
        config.enableMipmap      = enableMipmap;
        config.allowLinearFilter = allowLinearFilter;
        config.sampleCount       = sampleCount;
        return config;
    }

    RenderTargetConfig RenderTargetConfig::ColorTargetWithScale(
        const std::string& name,
        float              widthScale,
        float              heightScale,
        DXGI_FORMAT        format,
        bool               enableFlipper,
        LoadAction         loadAction,
        ClearValue         clearValue,
        bool               enableMipmap,
        bool               allowLinearFilter,
        int                sampleCount)
    {
        RenderTargetConfig config = ColorTarget(
            name,
            0, 0,
            format,
            enableFlipper,
            loadAction,
            clearValue,
            enableMipmap,
            allowLinearFilter,
            sampleCount
        );

        config.widthScale  = widthScale;
        config.heightScale = heightScale;

        return config;
    }

    RenderTargetConfig RenderTargetConfig::DepthTargetWithScale(
        const std::string& name,
        float              widthScale,
        float              heightScale,
        DXGI_FORMAT        format,
        LoadAction         loadAction,
        ClearValue         clearValue,
        int                sampleCount)
    {
        RenderTargetConfig config = DepthTarget(
            name,
            0, 0,
            format,
            false,
            loadAction,
            clearValue,
            false,
            true,
            sampleCount
        );

        config.widthScale  = widthScale;
        config.heightScale = heightScale;

        return config;
    }

    RenderTargetConfig RenderTargetConfig::ShadowColorTarget(
        const std::string& name,
        int                resolution,
        DXGI_FORMAT        format,
        bool               enableFlipper,
        LoadAction         loadAction,
        ClearValue         clearValue)
    {
        return ColorTarget(
            name,
            resolution, resolution,
            format,
            enableFlipper,
            loadAction,
            clearValue,
            false,
            true,
            1
        );
    }

    RenderTargetConfig RenderTargetConfig::ShadowDepthTarget(
        const std::string& name,
        int                resolution,
        DXGI_FORMAT        format,
        LoadAction         loadAction,
        ClearValue         clearValue)
    {
        return DepthTarget(
            name,
            resolution, resolution,
            format,
            false,
            loadAction,
            clearValue,
            false,
            true,
            1
        );
    }
} // namespace enigma::graphic
