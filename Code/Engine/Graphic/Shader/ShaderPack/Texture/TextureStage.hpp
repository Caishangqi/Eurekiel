#pragma once
#include <optional>
#include <string>

namespace enigma::graphic
{
    enum class TextureStage
    {
        /**
          * The setup passes.
          * <p>
          * Exclusive to Iris 1.6.
          */
        SETUP = 0,
        /**
         * The begin pass.
         * <p>
         * Exclusive to Iris 1.6.
         */
        BEGIN,
        /**
         * The shadowcomp passes.
         * <p>
         * While this is not documented in shaders.txt, it is a valid stage for defining custom textures.
         */
        SHADOWCOMP,
        /**
         * The prepare passes.
         * <p>
         * While this is not documented in shaders.txt, it is a valid stage for defining custom textures.
         */
        PREPARE,
        /**
         * All of the gbuffer passes, as well as the shadow passes.
         */
        GBUFFERS_AND_SHADOW,
        /**
         * The deferred pass.
         */
        DEFERRED,
        /** The debug pass. **/
        DEBUG,
        /**
         * The composite pass and final pass.
         */
        COMPOSITE_AND_FINAL
    };
}

using enigma::graphic::TextureStage;

inline std::optional<TextureStage> Parse(const std::string& name)
{
    if (name.empty())return std::nullopt;
    if (name == "setup") return TextureStage::SETUP;
    if (name == "begin") return TextureStage::BEGIN;
    if (name == "shadowcomp") return TextureStage::SHADOWCOMP;
    if (name == "prepare") return TextureStage::PREPARE;
    if (name == "gbuffers") return TextureStage::GBUFFERS_AND_SHADOW;
    if (name == "deferred") return TextureStage::DEFERRED;
    if (name == "composite") return TextureStage::COMPOSITE_AND_FINAL;
    return std::nullopt;
}
