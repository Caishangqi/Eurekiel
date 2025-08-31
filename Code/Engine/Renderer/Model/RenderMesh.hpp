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

namespace enigma::renderer::model
{
    using namespace enigma::voxel::property;
    
    /**
     * @brief Represents a single face of a block model
     */
    struct RenderFace
    {
        std::vector<Vertex_PCU> vertices;  // Usually 4 vertices for a quad
        std::vector<uint32_t> indices;     // Triangle indices (usually 6 for 2 triangles)
        Direction cullDirection;           // Direction this face can be culled against
        bool isOpaque = true;             // Whether this face is opaque
        int textureIndex = 0;             // Index in texture atlas
        
        RenderFace() = default;
        RenderFace(Direction dir) : cullDirection(dir) {}
        
        // Create a standard quad face
        void CreateQuad(const Vec3& bottomLeft, const Vec3& bottomRight, 
                       const Vec3& topRight, const Vec3& topLeft,
                       const Vec2& uvMin, const Vec2& uvMax,
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
    struct RenderMesh
    {
        std::vector<RenderFace> faces;           // All faces of this mesh
        
        // Compiled GPU data (cached)
        mutable std::shared_ptr<VertexBuffer> vertexBuffer;
        mutable std::shared_ptr<IndexBuffer> indexBuffer;
        mutable bool gpuDataValid = false;
        
        // Rendering properties
        bool isOpaque = true;
        bool isFullBlock = true;
        
        RenderMesh() = default;
        
        /**
         * @brief Add a face to this mesh
         */
        void AddFace(const RenderFace& face)
        {
            faces.push_back(face);
            gpuDataValid = false;
        }
        
        /**
         * @brief Get total vertex count across all faces
         */
        size_t GetVertexCount() const
        {
            size_t count = 0;
            for (const auto& face : faces)
            {
                count += face.vertices.size();
            }
            return count;
        }
        
        /**
         * @brief Get total index count across all faces
         */
        size_t GetIndexCount() const
        {
            size_t count = 0;
            for (const auto& face : faces)
            {
                count += face.indices.size();
            }
            return count;
        }
        
        /**
         * @brief Get face by direction (for culling)
         */
        const RenderFace* GetFace(Direction direction) const
        {
            for (const auto& face : faces)
            {
                if (face.cullDirection == direction)
                {
                    return &face;
                }
            }
            return nullptr;
        }
        
        /**
         * @brief Compile all faces into GPU buffers
         */
        void CompileToGPU() const;
        
        /**
         * @brief Get compiled vertex buffer (compiles if needed)
         */
        std::shared_ptr<VertexBuffer> GetVertexBuffer() const
        {
            if (!gpuDataValid)
            {
                CompileToGPU();
            }
            return vertexBuffer;
        }
        
        /**
         * @brief Get compiled index buffer (compiles if needed)
         */
        std::shared_ptr<IndexBuffer> GetIndexBuffer() const
        {
            if (!gpuDataValid)
            {
                CompileToGPU();
            }
            return indexBuffer;
        }
        
        /**
         * @brief Invalidate GPU data (call when mesh changes)
         */
        void InvalidateGPUData()
        {
            gpuDataValid = false;
        }
        
        /**
         * @brief Create a simple cube mesh (for testing)
         */
        static std::shared_ptr<RenderMesh> CreateCube(const Vec2& uvMin = Vec2(0, 0), const Vec2& uvMax = Vec2(1, 1));
        
        /**
         * @brief Check if this mesh has any faces
         */
        bool IsEmpty() const { return faces.empty(); }
        
        /**
         * @brief Get total triangle count
         */
        size_t GetTriangleCount() const { return GetIndexCount() / 3; }
        
        /**
         * @brief Clear all face data
         */
        void Clear()
        {
            faces.clear();
            gpuDataValid = false;
        }
    };
    
    using RenderMeshPtr = std::shared_ptr<RenderMesh>;
}
