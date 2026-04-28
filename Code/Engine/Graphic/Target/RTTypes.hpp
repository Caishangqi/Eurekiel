#pragma once

#include <cstdint>
#include <string>
#include <dxgi.h>
#include "Engine/Core/Rgba8.hpp"

namespace enigma::graphic
{
    // Render target provider categories used by the Iris-style pipeline.
    enum class RenderTargetType
    {
        ColorTex,
        ShadowColor,
        DepthTex,
        ShadowTex
    };

    // Render target load behavior at pass begin.
    enum class LoadAction
    {
        Load,
        Clear,
        DontCare
    };

    // Color or depth-stencil clear value.
    struct ClearValue
    {
        union
        {
            Rgba8 colorRgba8;
            struct DepthStencil
            {
                float   depth;
                uint8_t stencil;
            } depthStencil;
        };

        ClearValue() : colorRgba8(Rgba8::BLACK)
        {
        }

        ClearValue(const ClearValue& other) : colorRgba8(other.colorRgba8)
        {
        }

        ClearValue& operator=(const ClearValue& other)
        {
            if (this != &other)
            {
                colorRgba8 = other.colorRgba8;
            }
            return *this;
        }

        static ClearValue Color(const Rgba8& rgba8);
        static ClearValue Color(float r, float g, float b, float a = 1.0f);
        static ClearValue Depth(float depth, uint8_t stencil = 0);
        void GetColorAsFloats(float* outFloats) const;
    };

    // Render target creation and runtime resize configuration.
    struct RenderTargetConfig
    {
        std::string name          = "";
        int         width         = 0;
        int         height        = 0;
        DXGI_FORMAT format        = DXGI_FORMAT_R8G8B8A8_UNORM;
        bool        enableFlipper = true;
        LoadAction  loadAction    = LoadAction::Clear;
        ClearValue  clearValue;

        bool enableMipmap      = false;
        bool allowLinearFilter = true;
        int  sampleCount       = 1;

        float widthScale  = 1.0f;
        float heightScale = 1.0f;

        static RenderTargetConfig ColorTarget(
            const std::string& name,
            int                width, int height,
            DXGI_FORMAT        format,
            bool               enableFlipper     = true,
            LoadAction         loadAction        = LoadAction::Clear,
            ClearValue         clearValue        = ClearValue::Color(Rgba8::BLACK),
            bool               enableMipmap      = false,
            bool               allowLinearFilter = true,
            int                sampleCount       = 1
        );

        static RenderTargetConfig DepthTarget(
            const std::string& name,
            int                width, int height,
            DXGI_FORMAT        format,
            bool               enableFlipper     = false,
            LoadAction         loadAction        = LoadAction::Clear,
            ClearValue         clearValue        = ClearValue::Depth(1.0f, 0),
            bool               enableMipmap      = false,
            bool               allowLinearFilter = true,
            int                sampleCount       = 1
        );

        static RenderTargetConfig ColorTargetWithScale(
            const std::string& name,
            float              widthScale,
            float              heightScale,
            DXGI_FORMAT        format,
            bool               enableFlipper     = true,
            LoadAction         loadAction        = LoadAction::Clear,
            ClearValue         clearValue        = ClearValue::Color(Rgba8::BLACK),
            bool               enableMipmap      = false,
            bool               allowLinearFilter = true,
            int                sampleCount       = 1
        );

        static RenderTargetConfig DepthTargetWithScale(
            const std::string& name,
            float              widthScale,
            float              heightScale,
            DXGI_FORMAT        format,
            LoadAction         loadAction  = LoadAction::Clear,
            ClearValue         clearValue  = ClearValue::Depth(1.0f, 0),
            int                sampleCount = 1
        );

        // Shadow textures use square resolution.
        static RenderTargetConfig ShadowColorTarget(
            const std::string& name,
            int                resolution,
            DXGI_FORMAT        format        = DXGI_FORMAT_R8G8B8A8_UNORM,
            bool               enableFlipper = true,
            LoadAction         loadAction    = LoadAction::Clear,
            ClearValue         clearValue    = ClearValue::Color(Rgba8::WHITE)
        );

        static RenderTargetConfig ShadowDepthTarget(
            const std::string& name,
            int                resolution,
            DXGI_FORMAT        format     = DXGI_FORMAT_D32_FLOAT,
            LoadAction         loadAction = LoadAction::Clear,
            ClearValue         clearValue = ClearValue::Depth(1.0f, 0)
        );
    };
} // namespace enigma::graphic
