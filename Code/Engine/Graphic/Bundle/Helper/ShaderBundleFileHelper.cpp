//-----------------------------------------------------------------------------------------------
// ShaderBundleFileHelper.cpp
//
// [NEW] Implementation of ShaderBundle directory validation helpers
//
//-----------------------------------------------------------------------------------------------
#include "ShaderBundleFileHelper.hpp"
#include "Engine/Core/FileSystemHelper.hpp"

namespace enigma::graphic
{
    //-------------------------------------------------------------------------------------------
    // IsValidShaderBundleDirectory
    //-------------------------------------------------------------------------------------------
    bool ShaderBundleFileHelper::IsValidShaderBundleDirectory(const std::filesystem::path& directory)
    {
        // 1. Check directory existence first
        if (!FileSystemHelper::DirectoryExists(directory))
        {
            return false;
        }

        // 2. Check for shaders/bundle.json
        std::filesystem::path bundleJsonPath = FileSystemHelper::CombinePath(directory, "shaders/bundle.json");
        return FileSystemHelper::FileExists(bundleJsonPath);
    }

    //-------------------------------------------------------------------------------------------
    // HasRequiredStructure
    //-------------------------------------------------------------------------------------------
    bool ShaderBundleFileHelper::HasRequiredStructure(const std::filesystem::path& bundleRoot)
    {
        // 1. Check shaders/ directory exists
        std::filesystem::path shadersDir = FileSystemHelper::CombinePath(bundleRoot, "shaders");
        if (!FileSystemHelper::DirectoryExists(shadersDir))
        {
            return false;
        }

        // 2. Check bundle.json exists in shaders/
        std::filesystem::path bundleJsonPath = FileSystemHelper::CombinePath(shadersDir, "bundle.json");
        if (!FileSystemHelper::FileExists(bundleJsonPath))
        {
            return false;
        }

        // 3. Check for at least one of bundle/ or program/ directory
        //    At least one must exist for the bundle to contain any shaders
        std::filesystem::path bundleDir  = FileSystemHelper::CombinePath(shadersDir, "bundle");
        std::filesystem::path programDir = FileSystemHelper::CombinePath(shadersDir, "program");

        bool hasBundleDir  = FileSystemHelper::DirectoryExists(bundleDir);
        bool hasProgramDir = FileSystemHelper::DirectoryExists(programDir);

        // Return true only if at least one shader directory exists
        return hasBundleDir || hasProgramDir;
    }
} // namespace enigma::graphic
