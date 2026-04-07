#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace enigma::graphic
{
    struct ShaderCompileOptions
    {
        bool                               enableDebugInfo    = false;
        bool                               enableOptimization = true;
        bool                               enable16BitTypes   = true;
        bool                               enableBindless     = true;
        std::string                        entryPoint;
        std::vector<std::string>           defines;
        std::vector<std::filesystem::path> includePaths;

        static ShaderCompileOptions Default()
        {
            ShaderCompileOptions opts;
            return opts;
        }

        static ShaderCompileOptions Debug()
        {
            ShaderCompileOptions opts;
            opts.enableDebugInfo    = true;
            opts.enableOptimization = false;
            return opts;
        }

        static ShaderCompileOptions WithCommonInclude()
        {
            ShaderCompileOptions opts;
            opts.includePaths.emplace_back(
                std::filesystem::path("Run") / ".enigma" / "assets" / "engine" / "shaders" / "core");
            opts.entryPoint = "main";
            return opts;
        }
    };
}
