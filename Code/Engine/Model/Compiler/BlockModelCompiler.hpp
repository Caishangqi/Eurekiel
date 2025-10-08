#pragma once
#include "GenericModelCompiler.hpp"
#include "../../Renderer/Model/BlockRenderMesh.hpp"
#include "../../Resource/Atlas/TextureAtlas.hpp"
#include "../../Math/Vec2.hpp"

namespace enigma::renderer::model
{
    /**
     * @brief Specialized compiler for block models (parent: block/cube)
     * 
     * Handles standard Minecraft block cube models:
     * - 6-face cube geometry
     * - Texture variable resolution
     * - UV coordinate conversion (16x16 -> 0-1)
     * - Atlas sprite mapping
     */
    class BlockModelCompiler : public GenericModelCompiler
    {
    public:
        std::shared_ptr<RenderMesh> Compile(std::shared_ptr<resource::model::ModelResource> model, const CompilerContext& context) override;

    private:
        /**
         * @brief Compile a ModelElement into render faces
         * @param element Model element to compile
         * @param resolvedTextures Resolved texture mappings
         * @param blockMesh Target mesh to add faces to
         */
        void CompileElementToFaces(const resource::model::ModelElement&                     element,
                                   const std::map<std::string, resource::ResourceLocation>& resolvedTextures,
                                   std::shared_ptr<BlockRenderMesh>                         blockMesh);

        /**
         * @brief Create a render face from element and face data
         * @param direction Face direction
         * @param element Model element containing geometry
         * @param modelFace Face definition with UV and properties
         * @param uvMin Atlas UV minimum coordinates
         * @param uvMax Atlas UV maximum coordinates
         * @return Generated render face
         */
        RenderFace CreateElementFace(voxel::Direction                     direction,
                                     const resource::model::ModelElement& element,
                                     const resource::model::ModelFace&    modelFace,
                                     const Vec2&                          uvMin, const Vec2& uvMax) const;

        /**
         * @brief Get UV coordinates for a face from atlas
         * @param textureLocation Texture resource location
         * @return UV min/max in 0-1 coordinates
         */
        std::pair<Vec2, Vec2> GetAtlasUV(const resource::ResourceLocation& textureLocation) const;

        /**
         * @brief Create a face with proper UV mapping and colors
         */
        RenderFace CreateBlockFace(voxel::Direction                  direction,
                                   const resource::ResourceLocation& texture,
                                   const Vec2&                       uvMin, const Vec2& uvMax) const;
    };
}
