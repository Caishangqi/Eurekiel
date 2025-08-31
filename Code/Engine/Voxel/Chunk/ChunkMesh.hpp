#pragma once
#include "../../Core/Vertex_PCU.hpp"
#include "../../Renderer/VertexBuffer.hpp"
#include "../../Renderer/IndexBuffer.hpp"
#include <vector>
#include <memory>

class IRenderer;

namespace enigma::voxel::chunk
{
    /**
     * @brief Optimized mesh data for rendering an entire chunk
     * 
     * TODO: This class needs to be implemented with the following functionality:
     * 
     * CORE PURPOSE:
     * - Store compiled vertex/index data for all visible faces in a chunk
     * - Separate opaque and transparent geometry for proper rendering order
     * - Optimize for GPU rendering with minimal draw calls
     * 
     * REQUIRED MEMBER VARIABLES:
     * - std::vector<Vertex_PCU> m_opaqueVertices;          // Solid block faces
     * - std::vector<uint32_t> m_opaqueIndices;             // Indices for solid geometry
     * - std::vector<Vertex_PCU> m_transparentVertices;     // Transparent/translucent faces
     * - std::vector<uint32_t> m_transparentIndices;        // Indices for transparent geometry
     * 
     * GPU Resources (cached):
     * - std::shared_ptr<VertexBuffer> m_opaqueVertexBuffer;
     * - std::shared_ptr<IndexBuffer> m_opaqueIndexBuffer;
     * - std::shared_ptr<VertexBuffer> m_transparentVertexBuffer;
     * - std::shared_ptr<IndexBuffer> m_transparentIndexBuffer;
     * - bool m_gpuDataValid = false;                       // Whether GPU buffers are up to date
     * 
     * REQUIRED METHODS:
     * 
     * Data Management:
     * - void Clear()                                       // Clear all mesh data
     * - void AddOpaqueQuad(const std::array<Vertex_PCU, 4>& vertices)  // Add quad to opaque mesh
     * - void AddTransparentQuad(const std::array<Vertex_PCU, 4>& vertices)  // Add quad to transparent mesh
     * - void Reserve(size_t opaqueQuads, size_t transparentQuads)  // Pre-allocate space
     * 
     * Statistics:
     * - size_t GetOpaqueVertexCount() const
     * - size_t GetOpaqueTriangleCount() const
     * - size_t GetTransparentVertexCount() const
     * - size_t GetTransparentTriangleCount() const
     * - bool IsEmpty() const
     * - bool HasOpaqueGeometry() const
     * - bool HasTransparentGeometry() const
     * 
     * GPU Buffer Management:
     * - void CompileToGPU()                               // Upload data to GPU buffers
     * - void InvalidateGPUData()                          // Mark GPU buffers as outdated
     * - std::shared_ptr<VertexBuffer> GetOpaqueVertexBuffer()
     * - std::shared_ptr<IndexBuffer> GetOpaqueIndexBuffer()
     * - std::shared_ptr<VertexBuffer> GetTransparentVertexBuffer()
     * - std::shared_ptr<IndexBuffer> GetTransparentIndexBuffer()
     * 
     * Rendering Support:
     * - void RenderOpaque(IRenderer* renderer)            // Render solid geometry
     * - void RenderTransparent(IRenderer* renderer)      // Render transparent geometry
     * - void RenderAll(IRenderer* renderer)              // Render both passes
     * 
     * OPTIMIZATION FEATURES:
     * 
     * Face Culling:
     * - Only include faces that are visible (adjacent to air or transparent blocks)
     * - Cull faces between solid blocks of the same type
     * 
     * Greedy Meshing (Advanced):
     * - Combine adjacent faces of the same texture into larger quads
     * - Reduce vertex count for large flat surfaces
     * 
     * Ambient Occlusion:
     * - Calculate AO values for vertices based on neighboring blocks
     * - Store AO in vertex color or separate attribute
     * 
     * Texture Atlas Integration:
     * - Use UV coordinates that reference texture atlas
     * - Support for different texture layers/materials
     * 
     * MEMORY CONSIDERATIONS:
     * - Use memory pools for vertex/index data to reduce allocations
     * - Consider compression for sparse chunks
     * - Lazy GPU buffer creation (only when needed for rendering)
     * 
     * RENDERING INTEGRATION:
     * - Should work with existing DX11Renderer
     * - Support for frustum culling at chunk level
     * - Integration with lighting system (if implemented)
     * 
     * THREADING:
     * - Mesh building can happen on background thread
     * - GPU upload must happen on main thread
     * - Thread-safe state management during building
     */
    struct ChunkMesh
    {
        // TODO: Implement according to comments above

        ChunkMesh()  = default;
        ~ChunkMesh() = default;

        // Data Management:
        void Clear(); // Clear all mesh data
        void AddOpaqueQuad(const std::array<Vertex_PCU, 4>& vertices); // Add quad to opaque mesh
        void AddTransparentQuad(const std::array<Vertex_PCU, 4>& vertices); // Add quad to transparent mesh
        void Reserve(size_t opaqueQuads, size_t transparentQuads); // Pre-allocate space

        // Statistics:
        size_t GetOpaqueVertexCount() const;
        size_t GetOpaqueTriangleCount() const;
        size_t GetTransparentVertexCount() const;
        size_t GetTransparentTriangleCount() const;
        bool   IsEmpty() const;
        bool   HasOpaqueGeometry() const;
        bool   HasTransparentGeometry() const;

        // GPU Buffer Management:
        void                          CompileToGPU(); // Upload data to GPU buffers
        void                          InvalidateGPUData(); // Mark GPU buffers as outdated
        std::shared_ptr<VertexBuffer> GetOpaqueVertexBuffer();
        std::shared_ptr<IndexBuffer>  GetOpaqueIndexBuffer();
        std::shared_ptr<VertexBuffer> GetTransparentVertexBuffer();
        std::shared_ptr<IndexBuffer>  GetTransparentIndexBuffer();

        // Rendering:
        void RenderOpaque(IRenderer* renderer); // Render solid geometry
        void RenderTransparent(IRenderer* renderer); // Render transparent geometry
        void RenderAll(IRenderer* renderer); // Render both passes

    private:
        // Geometry Data
        std::vector<Vertex_PCU> m_opaqueVertices; // Solid block faces
        std::vector<uint32_t>   m_opaqueIndices; // Indices for solid geometry
        std::vector<Vertex_PCU> m_transparentVertices; // Transparent/translucent faces
        std::vector<uint32_t>   m_transparentIndices; // Indices for transparent geometry

        // GPU Resources
        std::shared_ptr<VertexBuffer> m_opaqueVertexBuffer;
        std::shared_ptr<IndexBuffer>  m_opaqueIndexBuffer;
        std::shared_ptr<VertexBuffer> m_transparentVertexBuffer;
        std::shared_ptr<IndexBuffer>  m_transparentIndexBuffer;
        bool                          m_gpuDataValid = false; // Whether GPU buffers are up to date
    };
}
