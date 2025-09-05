#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Engine/Voxel/Property/PropertyTypes.hpp"

namespace enigma::model
{
    class ModelSubsystem;
}

namespace enigma::resource
{
    class AtlasManager;
    class TextureAtlas;
    class ResourceLocation;
}

namespace enigma::resource::model
{
    struct ModelFace;
    struct ModelElement;
    class ModelResource;
}

namespace enigma::renderer::model
{
    struct RenderFace;
    class RenderMesh;

    struct CompilerContext
    {
        enigma::model::ModelSubsystem* modelSubsystem = nullptr; // For getting parent models
        const resource::AtlasManager*  atlasManager   = nullptr; // For texture UV lookup
        const resource::TextureAtlas*  blockAtlas     = nullptr; // Specific block texture atlas
        bool                           enableLogging  = false; // Debug logging

        CompilerContext(enigma::model::ModelSubsystem* subsystem, const resource::AtlasManager* atlas, bool logging = false);
        CompilerContext() = default;
    };

    class IModelCompiler
    {
    public:
        virtual                                         ~IModelCompiler() = default;
        virtual std::shared_ptr<RenderMesh>             Compile(std::shared_ptr<resource::model::ModelResource> model, const CompilerContext& context) = 0;
        virtual void                                    SetCompilerContext(const CompilerContext& context) = 0;
        virtual std::shared_ptr<resource::TextureAtlas> GetAtlas() const = 0;

    protected:
        virtual std::vector<RenderFace> CompileElements(const std::vector<resource::model::ModelElement>& elements, const std::map<std::string, resource::ResourceLocation>& resolvedTextures) = 0;
        virtual RenderFace              CompileFace(const std::string&                          faceDirection, const resource::model::ModelFace&, const resource::model::ModelElement& element,
                                       const std::map<std::string, resource::ResourceLocation>& resolvedTextures) = 0;
        virtual voxel::property::Direction StringToDirection(const std::string& direction) = 0;
        virtual void                       SetAtlas(std::shared_ptr<resource::TextureAtlas> atlas) = 0;
    };
}
