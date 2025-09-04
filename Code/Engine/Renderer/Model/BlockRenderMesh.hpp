#pragma once
#include "RenderMesh.hpp"
#include "../../Voxel/Chunk/ChunkMesh.hpp"
#include <array>

#include "Engine/Math/Vec4.hpp"

namespace enigma::voxel::chunk
{
    struct ChunkMesh;
}

namespace enigma::renderer::model
{
    using namespace enigma::voxel::chunk;

    /**
     * @brief Specialized render mesh for block models
     * 
     * Assignment 1 simplified version that inherits from RenderMesh:
     * - Uses parent's RenderFace system for 6 faces
     * - Provides simplified cube generation
     * - Integrates with ChunkMesh for world rendering
     * 
     * This inherits from RenderMesh to use the existing face-based system
     * while providing block-specific functionality.
     */
    class BlockRenderMesh : public RenderMesh
    {
    public:
        BlockRenderMesh() = default;

        /**
         * @brief Create a simple cube mesh with specified UV coordinates
         * 
         * Generates the standard block cube geometry (0-1 units):
         * - Down face (Y=0)
         * - Up face (Y=1)  
         * - North face (Z=0)
         * - South face (Z=1)
         * - West face (X=0)
         * - East face (X=1)
         * 
         * @param faceUVs UV coordinates for each face [down, up, north, south, west, east]
         * @param faceColors Colors for each face (default white)
         */
        void CreateCube(const std::array<Vec4, 6>&  faceUVs,
                        const std::array<Rgba8, 6>& faceColors = {
                            Rgba8::WHITE, Rgba8::WHITE, Rgba8::WHITE,
                            Rgba8::WHITE, Rgba8::WHITE, Rgba8::WHITE
                        });

        /**
         * @brief Transform and append this mesh's faces to a chunk mesh
         * 
         * Transforms all face vertices by the block position and adds them to the chunk mesh.
         * Uses the parent class's RenderFace system.
         * 
         * @param chunkMesh Target chunk mesh to append to
         * @param blockPos World position of the block (will be used to offset vertices)
         */
        void TransformAndAppendTo(ChunkMesh* chunkMesh, const Vec3& blockPos) const;

        /**
         * @brief Create a cube mesh with uniform UV and color
         * Convenience method for simple blocks
         */
        void CreateSimpleCube(const Vec4& uv = Vec4(0, 0, 1, 1), const Rgba8& color = Rgba8::WHITE);

        /**
         * @brief Get specific face by direction (using parent's system)
         * Convenience method that uses parent class functionality
         */
        const RenderFace* GetBlockFace(Direction direction) const { return GetFace(direction); }

    private:
        /**
         * @brief Create UV coordinates from Vec4 (minX, minY, maxX, maxY)
         */
        Vec2 GetUVMin(const Vec4& uv) const { return Vec2(uv.x, uv.y); }
        Vec2 GetUVMax(const Vec4& uv) const { return Vec2(uv.z, uv.w); }
    };

    using BlockRenderMeshPtr = std::shared_ptr<BlockRenderMesh>;
}
