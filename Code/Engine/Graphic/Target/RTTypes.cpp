#include "Engine/Graphic/Target/RTTypes.hpp"

namespace enigma::graphic
{
    // ============================================================================  
    // ClearValue 静态工厂方法实现
    // ============================================================================

    ClearValue ClearValue::Color(const Rgba8& rgba8)
    {
        ClearValue cv;
        cv.colorRgba8 = rgba8;
        return cv;
    }

    ClearValue ClearValue::Color(float r, float g, float b, float a)
    {
        // 将浮点值 [0-1] 转换为Rgba8 [0-255]
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
        // 使用Rgba8::GetAsFloats()转换为D3D12 API所需的float数组
        colorRgba8.GetAsFloats(outFloats);
    }

    // ============================================================================  
    // RTConfig 静态工厂方法实现
    // ============================================================================

    RTConfig RTConfig::ColorTarget(
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
        RTConfig config;
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

    RTConfig RTConfig::DepthTarget(
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
        RTConfig config;
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

    RTConfig RTConfig::ColorTargetWithScale(
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
        // 调用ColorTarget()创建基础配置，width和height暂时设为0
        // 实际尺寸由RenderTargetManager构造时根据widthScale/heightScale计算
        RTConfig config = ColorTarget(
            name,
            0, 0, // width和height暂时设为0
            format,
            enableFlipper,
            loadAction,
            clearValue,
            enableMipmap,
            allowLinearFilter,
            sampleCount
        );

        // 设置相对缩放因子
        config.widthScale  = widthScale;
        config.heightScale = heightScale;

        return config;
    }

    RTConfig RTConfig::DepthTargetWithScale(
        const std::string& name,
        float              widthScale,
        float              heightScale,
        DXGI_FORMAT        format,
        LoadAction         loadAction,
        ClearValue         clearValue,
        int                sampleCount)
    {
        // 调用DepthTarget()创建基础配置，width和height暂时设为0
        // 实际尺寸由RenderTargetManager构造时根据widthScale/heightScale计算
        // 深度纹理固定参数：enableFlipper=false, enableMipmap=false, allowLinearFilter=true
        RTConfig config = DepthTarget(
            name,
            0, 0, // width和height暂时设为0
            format,
            false, // enableFlipper - 深度纹理通常不启用翻转
            loadAction,
            clearValue,
            false, // enableMipmap - 深度纹理通常不启用Mipmap
            true, // allowLinearFilter - 允许线性过滤
            sampleCount
        );

        // 设置相对缩放因子
        config.widthScale  = widthScale;
        config.heightScale = heightScale;

        return config;
    }
} // namespace enigma::graphic
