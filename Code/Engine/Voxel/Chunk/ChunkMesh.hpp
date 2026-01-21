#pragma once
#include <vector>
#include <memory>
#include <array>

#include "Engine/Graphic/Resource/Buffer/D12IndexBuffer.hpp"
#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"
#include "Engine/Voxel/World/TerrainVertexLayout.hpp"


namespace enigma::voxel
{
    /**
     * @brief Chunk mesh data holder for DX12 deferred rendering
     * 
     * Pure data class following SRP - stores vertex/index data for terrain rendering.
     * TerrainRenderPass accesses GPU buffers directly via GetXxxD12Buffer() methods.
     * 
     * [REFACTORED] Now supports three render types:
     * - Opaque: Fully opaque blocks (stone, dirt, etc.)
     * - Cutout: Alpha-tested blocks (leaves, grass) - no depth sorting needed
     * - Translucent: Alpha-blended blocks (water, glass) - requires depth sorting
     * 
     * [MINECRAFT REF] ItemBlockRenderTypes / RenderType classification
     */
    struct ChunkMesh
    {
        ChunkMesh()  = default;
        ~ChunkMesh() = default;

        // Data Management
        void Clear();
        void AddOpaqueTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices);
        void AddCutoutTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices);
        void AddTranslucentTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices);
        void Reserve(size_t opaqueQuads, size_t cutoutQuads, size_t translucentQuads);

        // [DEPRECATED] Legacy API - routes to Translucent for backward compatibility
        void AddTransparentTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices);

        // Statistics - Opaque
        size_t GetOpaqueVertexCount() const;
        size_t GetOpaqueIndexCount() const;
        size_t GetOpaqueTriangleCount() const;
        bool   HasOpaqueGeometry() const;

        // Statistics - Cutout
        size_t GetCutoutVertexCount() const;
        size_t GetCutoutIndexCount() const;
        size_t GetCutoutTriangleCount() const;
        bool   HasCutoutGeometry() const;

        // Statistics - Translucent
        size_t GetTranslucentVertexCount() const;
        size_t GetTranslucentIndexCount() const;
        size_t GetTranslucentTriangleCount() const;
        bool   HasTranslucentGeometry() const;

        // [DEPRECATED] Legacy API - returns Translucent counts
        size_t GetTransparentVertexCount() const;
        size_t GetTransparentIndexCount() const;
        size_t GetTransparentTriangleCount() const;
        bool   HasTransparentGeometry() const;

        bool IsEmpty() const;

        // GPU Buffer Management
        void CompileToGPU();
        void InvalidateGPUData();

        // DX12 Buffer Access - Opaque (used by TerrainRenderPass)
        [[nodiscard]] std::shared_ptr<graphic::D12VertexBuffer> GetOpaqueD12VertexBuffer() { return m_d12OpaqueVertexBuffer; }
        [[nodiscard]] std::shared_ptr<graphic::D12IndexBuffer>  GetOpaqueD12IndexBuffer() { return m_d12OpaqueIndexBuffer; }

        // DX12 Buffer Access - Cutout
        [[nodiscard]] std::shared_ptr<graphic::D12VertexBuffer> GetCutoutD12VertexBuffer() { return m_d12CutoutVertexBuffer; }
        [[nodiscard]] std::shared_ptr<graphic::D12IndexBuffer>  GetCutoutD12IndexBuffer() { return m_d12CutoutIndexBuffer; }

        // DX12 Buffer Access - Translucent
        [[nodiscard]] std::shared_ptr<graphic::D12VertexBuffer> GetTranslucentD12VertexBuffer() { return m_d12TranslucentVertexBuffer; }
        [[nodiscard]] std::shared_ptr<graphic::D12IndexBuffer>  GetTranslucentD12IndexBuffer() { return m_d12TranslucentIndexBuffer; }

        // [DEPRECATED] Legacy API - returns Translucent buffers
        [[nodiscard]] std::shared_ptr<graphic::D12VertexBuffer> GetTransparentD12VertexBuffer() { return m_d12TranslucentVertexBuffer; }
        [[nodiscard]] std::shared_ptr<graphic::D12IndexBuffer>  GetTransparentD12IndexBuffer() { return m_d12TranslucentIndexBuffer; }

        // Raw vertex data access (for mesh building)
        std::vector<graphic::TerrainVertex>& GetOpaqueTerrainVertices() { return m_opaqueTerrainVertices; }
        std::vector<graphic::TerrainVertex>& GetCutoutTerrainVertices() { return m_cutoutTerrainVertices; }
        std::vector<graphic::TerrainVertex>& GetTranslucentTerrainVertices() { return m_translucentTerrainVertices; }
        // [DEPRECATED] Legacy API
        std::vector<graphic::TerrainVertex>& GetTransparentTerrainVertices() { return m_translucentTerrainVertices; }

    private:
        // TerrainVertex geometry data - Three render types
        std::vector<graphic::TerrainVertex> m_opaqueTerrainVertices;
        std::vector<graphic::TerrainVertex> m_cutoutTerrainVertices;
        std::vector<graphic::TerrainVertex> m_translucentTerrainVertices;
        std::vector<uint32_t>               m_opaqueIndices;
        std::vector<uint32_t>               m_cutoutIndices;
        std::vector<uint32_t>               m_translucentIndices;

        // DX12 GPU Resources - Three render types
        std::shared_ptr<graphic::D12VertexBuffer> m_d12OpaqueVertexBuffer;
        std::shared_ptr<graphic::D12VertexBuffer> m_d12CutoutVertexBuffer;
        std::shared_ptr<graphic::D12VertexBuffer> m_d12TranslucentVertexBuffer;
        std::shared_ptr<graphic::D12IndexBuffer>  m_d12OpaqueIndexBuffer;
        std::shared_ptr<graphic::D12IndexBuffer>  m_d12CutoutIndexBuffer;
        std::shared_ptr<graphic::D12IndexBuffer>  m_d12TranslucentIndexBuffer;
        bool                                      m_gpuDataValid = false;
    };
}
