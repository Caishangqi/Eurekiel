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
     */
    struct ChunkMesh
    {
        ChunkMesh()  = default;
        ~ChunkMesh() = default;

        // Data Management
        void Clear();
        void AddOpaqueTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices);
        void AddTransparentTerrainQuad(const std::array<graphic::TerrainVertex, 4>& vertices);
        void Reserve(size_t opaqueQuads, size_t transparentQuads);

        // Statistics
        size_t GetOpaqueVertexCount() const;
        size_t GetOpaqueIndexCount() const;
        size_t GetOpaqueTriangleCount() const;
        size_t GetTransparentVertexCount() const;
        size_t GetTransparentIndexCount() const;
        size_t GetTransparentTriangleCount() const;
        bool   IsEmpty() const;
        bool   HasOpaqueGeometry() const;
        bool   HasTransparentGeometry() const;

        // GPU Buffer Management
        void CompileToGPU();
        void InvalidateGPUData();

        // DX12 Buffer Access (used by TerrainRenderPass)
        [[nodiscard]] std::shared_ptr<graphic::D12VertexBuffer> GetOpaqueD12VertexBuffer() { return m_d12OpaqueVertexBuffer; }
        [[nodiscard]] std::shared_ptr<graphic::D12IndexBuffer>  GetOpaqueD12IndexBuffer() { return m_d12OpaqueIndexBuffer; }
        [[nodiscard]] std::shared_ptr<graphic::D12VertexBuffer> GetTransparentD12VertexBuffer() { return m_d12TransparentVertexBuffer; }
        [[nodiscard]] std::shared_ptr<graphic::D12IndexBuffer>  GetTransparentD12IndexBuffer() { return m_d12TransparentIndexBuffer; }

        // Raw vertex data access (for mesh building)
        std::vector<graphic::TerrainVertex>& GetOpaqueTerrainVertices() { return m_opaqueTerrainVertices; }
        std::vector<graphic::TerrainVertex>& GetTransparentTerrainVertices() { return m_transparentTerrainVertices; }

    private:
        // TerrainVertex geometry data
        std::vector<graphic::TerrainVertex> m_opaqueTerrainVertices;
        std::vector<graphic::TerrainVertex> m_transparentTerrainVertices;
        std::vector<uint32_t>               m_opaqueIndices;
        std::vector<uint32_t>               m_transparentIndices;

        // DX12 GPU Resources
        std::shared_ptr<graphic::D12VertexBuffer> m_d12OpaqueVertexBuffer;
        std::shared_ptr<graphic::D12VertexBuffer> m_d12TransparentVertexBuffer;
        std::shared_ptr<graphic::D12IndexBuffer>  m_d12OpaqueIndexBuffer;
        std::shared_ptr<graphic::D12IndexBuffer>  m_d12TransparentIndexBuffer;
        bool                                      m_gpuDataValid = false;
    };
}
