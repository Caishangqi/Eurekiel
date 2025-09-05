#include "IModelCompiler.hpp"

#include "Engine/Resource/Atlas/AtlasManager.hpp"

using namespace enigma::renderer::model;

CompilerContext::CompilerContext(enigma::model::ModelSubsystem* subsystem, const resource::AtlasManager* atlas, bool logging)
    : modelSubsystem(subsystem), atlasManager(atlas), enableLogging(logging)
{
    // Get the blocks atlas specifically
    if (atlasManager)
    {
        blockAtlas = atlasManager->GetAtlas("blocks");
    }
}
