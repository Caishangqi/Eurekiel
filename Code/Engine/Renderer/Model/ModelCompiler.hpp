#pragma once
#include "RenderMesh.hpp"
#include "../../Resource/Model/ModelResource.hpp"
#include "../../Resource/Atlas/TextureAtlas.hpp"
#include <memory>
#include <unordered_map>
#include <string>

#include "Engine/Voxel/Chunk/ChunkMeshBuilder.hpp"

namespace enigma::renderer::model
{
    using namespace enigma::resource::model;
    using namespace enigma::resource;

    /**
     * @brief Compiles ModelResource into RenderMesh for GPU rendering
     * 
     * Handles parent inheritance, texture resolution, and UV remapping to atlas coordinates.
     */
    class ModelCompiler
    {
    private:
        std::shared_ptr<TextureAtlas> m_atlas;

        // Cache for compiled meshes
        mutable std::unordered_map<std::string, std::weak_ptr<RenderMesh>> m_meshCache;

    public:
        explicit ModelCompiler(std::shared_ptr<TextureAtlas> atlas)
            : m_atlas(std::move(atlas))
        {
        }

        /**
         * @brief Compile a ModelResource to RenderMesh
         */
        std::shared_ptr<RenderMesh> Compile(std::shared_ptr<ModelResource> model);

        /**
         * @brief Compile with caching - reuses existing mesh if available
         */
        std::shared_ptr<RenderMesh> CompileWithCache(std::shared_ptr<ModelResource> model);

        /**
         * @brief Clear the mesh cache
         */
        void ClearCache() { m_meshCache.clear(); }

        /**
         * @brief Set the texture atlas to use
         */
        void SetAtlas(std::shared_ptr<TextureAtlas> atlas) { m_atlas = std::move(atlas); }

        /**
         * @brief Get the current texture atlas
         */
        std::shared_ptr<TextureAtlas> GetAtlas() const { return m_atlas; }

    private:
        /**
         * @brief Compile model elements to render faces
         */
        std::vector<RenderFace> CompileElements(const std::vector<ModelElement>&               elements,
                                                const std::map<std::string, ResourceLocation>& resolvedTextures);

        /**
         * @brief Compile a single model element
         */
        std::vector<RenderFace> CompileElement(const ModelElement&                            element,
                                               const std::map<std::string, ResourceLocation>& resolvedTextures);

        /**
         * @brief Compile a single face of an element
         */
        RenderFace CompileFace(const std::string&                             faceDirection,
                               const ModelFace&                               face,
                               const ModelElement&                            element,
                               const std::map<std::string, ResourceLocation>& resolvedTextures);

        /**
         * @brief Resolve texture variable to actual texture path
         */
        ResourceLocation ResolveTextureVariable(const std::string&                             textureVar,
                                                const std::map<std::string, ResourceLocation>& resolvedTextures);

        /**
         * @brief Get UV coordinates from atlas for a texture
         */
        std::pair<Vec2, Vec2> GetAtlasUV(const ResourceLocation& texturePath, const Vec4& modelUV);

        /**
         * @brief Convert face direction string to Direction enum
         */
        Direction StringToDirection(const std::string& direction);

        /**
         * @brief Create vertices for a face based on direction and element bounds
         */
        std::vector<Vertex_PCU> CreateFaceVertices(Direction   faceDir,
                                                   const Vec3& from,
                                                   const Vec3& to,
                                                   const Vec2& uvMin,
                                                   const Vec2& uvMax,
                                                   int         rotation = 0);

        /**
         * @brief Apply UV rotation to texture coordinates
         */
        Vec2 RotateUV(const Vec2& uv, int rotation);

        /**
         * @brief Generate cache key for a model
         */
        std::string GenerateCacheKey(std::shared_ptr<ModelResource> model);
    };
}
