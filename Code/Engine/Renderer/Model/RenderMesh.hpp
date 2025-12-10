#pragma once
#include "../../Core/Vertex_PCU.hpp"
#include "../../Math/Vec2.hpp"
#include "../../Math/Vec3.hpp"
#include "../VertexBuffer.hpp"
#include "../IndexBuffer.hpp"
#include "../../Voxel/Property/PropertyTypes.hpp"
#include <vector>
#include <memory>
#include <array>

#include "Engine/Voxel/Chunk/ChunkMesh.hpp"

namespace enigma::renderer::model
{
    using namespace enigma::voxel;

    /**
     * @brief Represents a single face of a block model
     */
    struct RenderFace
    {
        std::vector<Vertex_PCU> vertices; // Usually 4 vertices for a quad
        std::vector<uint32_t>   indices; // Triangle indices (usually 6 for 2 triangles)
        Direction               cullDirection = Direction::NORTH; // Direction this face can be culled against
        bool                    isOpaque      = true; // Whether this face is opaque
        int                     textureIndex  = 0; // Index in texture atlas

        RenderFace() = default;

        RenderFace(Direction dir) : cullDirection(dir)
        {
        }

        // Create a standard quad face
        void CreateQuad(const Vec3&  bottomLeft, const Vec3& bottomRight,
                        const Vec3&  topRight, const Vec3&   topLeft,
                        const Vec2&  uvMin, const Vec2&      uvMax,
                        const Rgba8& color = Rgba8::WHITE);

        // Helper to create faces for standard block sides
        static RenderFace CreateNorthFace(const Vec3& blockPos, const Vec2& uvMin, const Vec2& uvMax);
        static RenderFace CreateSouthFace(const Vec3& blockPos, const Vec2& uvMin, const Vec2& uvMax);
        static RenderFace CreateEastFace(const Vec3& blockPos, const Vec2& uvMin, const Vec2& uvMax);
        static RenderFace CreateWestFace(const Vec3& blockPos, const Vec2& uvMin, const Vec2& uvMax);
        static RenderFace CreateUpFace(const Vec3& blockPos, const Vec2& uvMin, const Vec2& uvMax);
        static RenderFace CreateDownFace(const Vec3& blockPos, const Vec2& uvMin, const Vec2& uvMax);
    };

    /**
     * @brief Compiled mesh data for rendering blocks
     * 
     * Contains vertex and index data ready for GPU rendering.
     * Represents a single block's geometry in model space (0-1 cube).
     */
    class RenderMesh
    {
    public:
        std::vector<RenderFace> faces; // All faces of this mesh

        // Compiled GPU data (cached)
    private:
        mutable std::shared_ptr<VertexBuffer> vertexBuffer;
        mutable std::shared_ptr<IndexBuffer>  indexBuffer;
        mutable bool                          gpuDataValid = false;

        // Rendering properties
        bool isOpaque    = true;
        bool isFullBlock = true;

    public:
        RenderMesh() = default;

        /**
         * @brief Add a face to this mesh
         */
        void AddFace(const RenderFace& face);

        /**
         * @brief Get total vertex count across all faces
         */
        size_t GetVertexCount() const;

        /**
         * @brief Get total index count across all faces
         */
        size_t GetIndexCount() const;

        /**
         * @brief Get first face by direction (for culling)
         * @note For multi-element models (like stairs), use GetFaces() instead
         */
        const RenderFace* GetFace(Direction direction) const;

        /**
         * @brief Get all faces by direction (for multi-element models like stairs)
         *
         * Unlike GetFace() which only returns the first match, this method returns
         * all faces with the specified cull direction. Essential for models with
         * multiple elements (e.g., stairs have 2 elements = up to 11 faces).
         *
         * @param direction The direction to filter faces by
         * @return Vector of pointers to all faces with matching cullDirection
         */
        std::vector<const RenderFace*> GetFaces(Direction direction) const;

        /**
         * @brief Compile all faces into GPU buffers
         */
        void CompileToGPU() const;

        /**
         * @brief Get compiled vertex buffer (compiles if needed)
         */
        std::shared_ptr<VertexBuffer> GetVertexBuffer() const;

        /**
         * @brief Get compiled index buffer (compiles if needed)
         */
        std::shared_ptr<IndexBuffer> GetIndexBuffer() const;

        /**
         * @brief Invalidate GPU data (call when mesh changes)
         */
        void InvalidateGPUData();
        void TransformAndAppendTo(voxel::ChunkMesh* chunk_mesh, const Vec3& vec3);

        /**
         * @brief Create a simple cube mesh (for testing)
         */
        [[deprecated]] static std::shared_ptr<RenderMesh> CreateCube(const Vec2& uvMin = Vec2(0, 0), const Vec2& uvMax = Vec2(1, 1));

        /**
         * @brief Check if this mesh has any faces
         */
        bool IsEmpty() const { return faces.empty(); }

        /**
         * @brief Apply block rotation to all vertices in this mesh
         *
         * Rotates all vertices around the block center (0.5, 0.5, 0.5).
         * Used for blockstate variant rotations (0, 90, 180, 270 degrees).
         *
         * Coordinate system mapping:
         * - SimpleMiner: +X=Forward, +Y=Left, +Z=Up
         * - Minecraft:   +X=East,    +Y=Up,   +Z=South
         *
         * @param rotX Rotation around X axis in degrees (Minecraft's X rotation)
         *             Maps to rotation around SimpleMiner's X axis
         * @param rotY Rotation around Y axis in degrees (Minecraft's Y rotation)
         *             Maps to rotation around SimpleMiner's Z axis (up)
         */
        void ApplyBlockRotation(int rotX, int rotY);

        /**
         * @brief Get total triangle count
         */
        size_t GetTriangleCount() const { return GetIndexCount() / 3; }

        /**
         * @brief Clear all face data
         */
        void Clear();
    };

    using RenderMeshPtr = std::shared_ptr<RenderMesh>;
}
