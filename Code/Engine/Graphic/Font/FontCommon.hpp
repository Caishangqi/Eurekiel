#pragma once

#include <string_view>

#include "Engine/Core/LogCategory/LogCategory.hpp"

namespace enigma::graphic
{
    DECLARE_LOG_CATEGORY_EXTERN(LogFont);

    inline constexpr std::string_view kFontAssetDirectoryName = "font";
    inline constexpr std::string_view kFontShaderAssetDirectoryName = "shaders/font";
    inline constexpr std::string_view kEngineAssetNamespace = "engine";
    inline constexpr std::string_view kDefaultGameAssetNamespace = "game";

    // M1 path contract:
    // - Engine .ttf source: F:/p4/Personal/SD/Engine/.enigma/assets/engine/font/
    // - Runtime Engine .ttf: F:/p4/Personal/SD/FancyFonts/Run/.enigma/assets/engine/font/
    // - Runtime App/User .ttf: F:/p4/Personal/SD/FancyFonts/Run/.enigma/assets/<namespace>/font/
    // - Font shaders stay separate under .../assets/<namespace>/shaders/font/
} // namespace enigma::graphic
