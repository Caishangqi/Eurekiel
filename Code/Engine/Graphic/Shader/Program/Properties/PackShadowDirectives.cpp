#include "Engine/Graphic/Shader/Program/Properties/PackShadowDirectives.hpp"

#include "Engine/Core/EngineCommon.hpp"
using namespace enigma::graphic;

void PackShadowDirectives::Reset()
{
    shadowMapResolution          = 2048;
    shadowEnabled                = true;
    shadowColorFormat            = DXGI_FORMAT_R8G8B8A8_UNORM;
    shadowHardwareFiltering      = false;
    shadowColorMipmap            = false;
    shadowColorClear             = false;
    shadowDepthFormat            = DXGI_FORMAT_D32_FLOAT;
    shadowDepthHardwareFiltering = true;
    shadowDepthMipmap            = false;
    shadowDistance               = 120.0f;
    shadowClipFrustum            = true;
    shadowSamples                = 1;
}

DXGI_FORMAT PackShadowDirectives::GetShadowColorFormat(int index) const
{
    // 占位符: 忽略index，所有shadowcolor使用统一格式
    UNUSED(index)
    return shadowColorFormat;
}

bool PackShadowDirectives::IsShadowColorHardwareFiltered(int index) const
{
    UNUSED(index)
    // 占位符: 忽略index，所有shadowcolor使用统一配置
    return shadowHardwareFiltering;
}

bool PackShadowDirectives::IsShadowColorMipmapEnabled(int index) const
{
    UNUSED(index)
    // 占位符: 忽略index，所有shadowcolor使用统一配置
    return shadowColorMipmap;
}

bool PackShadowDirectives::ShouldShadowColorClearEveryFrame(int index) const
{
    UNUSED(index)
    // 占位符: 忽略index，所有shadowcolor使用统一配置
    return shadowColorClear;
}

PackShadowDirectives PackShadowDirectives::Parse(const std::string& propertiesContent)
{
    UNUSED(propertiesContent)
    // ⚠️ 占位符: 返回默认配置，未实现解析逻辑
    return PackShadowDirectives();
}

std::string PackShadowDirectives::GetDebugInfo() const
{
    std::string info;
    info += "PackShadowDirectives (Placeholder):\n";
    info += "  shadowMapResolution: " + std::to_string(shadowMapResolution) + "\n";
    info += "  shadowEnabled: " + std::string(shadowEnabled ? "true" : "false") + "\n";
    info += "  shadowColorFormat: RGBA8 (unified)\n";
    info += "  shadowHardwareFiltering: " + std::string(shadowHardwareFiltering ? "true" : "false") + "\n";
    info += "  shadowColorMipmap: " + std::string(shadowColorMipmap ? "true" : "false") + "\n";
    info += "  shadowColorClear: " + std::string(shadowColorClear ? "true" : "false") + "\n";
    info += "  shadowDistance: " + std::to_string(shadowDistance) + "\n";
    return info;
}
