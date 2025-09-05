#pragma once
#include "IModelCompiler.hpp"

namespace enigma::renderer::model
{
    class GenericModelCompiler : public IModelCompiler
    {
    public:
        explicit                                GenericModelCompiler();
        std::shared_ptr<RenderMesh>             Compile(std::shared_ptr<resource::model::ModelResource> model, const CompilerContext& context) override;
        void                                    SetCompilerContext(const CompilerContext& context) override;
        std::shared_ptr<resource::TextureAtlas> GetAtlas() const override;

    protected:
        std::vector<RenderFace> CompileElements(const std::vector<resource::model::ModelElement>& elements, const std::map<std::string, resource::ResourceLocation>& resolvedTextures) override;
        RenderFace              CompileFace(const std::string&                          faceDirection, const resource::model::ModelFace&, const resource::model::ModelElement& element,
                               const std::map<std::string, resource::ResourceLocation>& resolvedTextures) override;
        voxel::property::Direction StringToDirection(const std::string& direction) override;
        void                       SetAtlas(std::shared_ptr<resource::TextureAtlas> atlas) override;

    protected:
        std::shared_ptr<resource::TextureAtlas> m_atlas; // Texture atlas that used in RenderMesh
        std::shared_ptr<RenderMesh>             m_mesh; // Cached mesh in processing
        CompilerContext                         m_context; // Compiler context
    };
}
