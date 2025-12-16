#pragma once
#include <string>

namespace enigma::graphic
{
    struct ShaderBundleSubsystemConfiguration
    {
        std::string shaderBundleUserDiscoveryPath = ".enigma\\shaderbundles";
        std::string shaderBundleEnginePath        = ".enigma\\assets\\engine\\shaders";
    };
}
