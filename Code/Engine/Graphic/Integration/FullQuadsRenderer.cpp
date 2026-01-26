#include "FullQuadsRenderer.hpp"

#include "RendererSubsystem.hpp"
#include "Engine/Core/VertexUtils.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Graphic/Helper/VertexConversionHelper.hpp"
using namespace enigma::graphic;

std::shared_ptr<D12VertexBuffer> FullQuadsRenderer::m_fullQuadsVertexBuffer = nullptr;

void FullQuadsRenderer::DrawFullQuads()
{
    g_theRendererSubsystem->DrawVertexBuffer(m_fullQuadsVertexBuffer);
}

FullQuadsRenderer::FullQuadsRenderer()
{
    /// Fullquads vertex buffer
    std::vector<Vertex_PCU> verts;
    AABB2                   fullQuadsAABB2;
    fullQuadsAABB2.m_mins = Vec2(-1, -1);
    fullQuadsAABB2.m_maxs = Vec2(1.0, 1.0f);
    AddVertsForAABB2D(verts, fullQuadsAABB2, Rgba8::WHITE);
    auto vertsTBN             = VertexConversionHelper::ToPCUTBNVector(verts);
    vertsTBN[0].m_uvTexCoords = Vec2(0.0f, 1.0f);
    vertsTBN[1].m_uvTexCoords = Vec2(1.0f, 0.0f);
    vertsTBN[2].m_uvTexCoords = Vec2(0.0f, 0.0f);
    vertsTBN[3].m_uvTexCoords = Vec2(0.0f, 1.0f);
    vertsTBN[4].m_uvTexCoords = Vec2(1.0f, 1.0f);
    vertsTBN[5].m_uvTexCoords = Vec2(1.0f, 0.0f);
    m_fullQuadsVertexBuffer   = D3D12RenderSystem::CreateVertexBuffer(vertsTBN.size() * sizeof(Vertex_PCUTBN), sizeof(Vertex_PCUTBN), vertsTBN.data());
}

FullQuadsRenderer::~FullQuadsRenderer()
{
    m_fullQuadsVertexBuffer.reset();
}
