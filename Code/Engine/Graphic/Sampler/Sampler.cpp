#include "Sampler.hpp"
#include "SamplerProviderException.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"

// ============================================================================
// Sampler.cpp - [NEW] RAII Sampler implementation
// Part of Dynamic Sampler System
// ============================================================================

namespace enigma::graphic
{
    Sampler::Sampler(GlobalDescriptorHeapManager& heapManager, const SamplerConfig& config)
        : m_heapManager(&heapManager)
          , m_config(config)
    {
        // Allocate sampler descriptor from heap
        m_allocation = m_heapManager->AllocateSampler();

        if (!m_allocation.isValid)
        {
            throw SamplerHeapAllocationException("Failed to allocate sampler descriptor from heap");
        }

        m_bindlessIndex = m_allocation.heapIndex;

        // Create D3D12 sampler descriptor
        CreateSamplerDescriptor();
    }

    Sampler::~Sampler()
    {
        Release();
    }

    Sampler::Sampler(Sampler&& other) noexcept
        : m_heapManager(other.m_heapManager)
          , m_allocation(other.m_allocation)
          , m_config(other.m_config)
          , m_bindlessIndex(other.m_bindlessIndex)
    {
        // Invalidate source
        other.m_heapManager = nullptr;
        other.m_allocation.Reset();
        other.m_bindlessIndex = INVALID_SAMPLER_INDEX;
    }

    Sampler& Sampler::operator=(Sampler&& other) noexcept
    {
        if (this != &other)
        {
            // Release current resources
            Release();

            // Transfer ownership
            m_heapManager   = other.m_heapManager;
            m_allocation    = other.m_allocation;
            m_config        = other.m_config;
            m_bindlessIndex = other.m_bindlessIndex;

            // Invalidate source
            other.m_heapManager = nullptr;
            other.m_allocation.Reset();
            other.m_bindlessIndex = INVALID_SAMPLER_INDEX;
        }
        return *this;
    }

    void Sampler::UpdateConfig(const SamplerConfig& config)
    {
        if (!IsValid())
        {
            return;
        }

        m_config = config;

        // Recreate descriptor with new config
        CreateSamplerDescriptor();
    }

    void Sampler::CreateSamplerDescriptor()
    {
        if (!m_heapManager || m_bindlessIndex == INVALID_SAMPLER_INDEX)
        {
            return;
        }

        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            return;
        }

        // Build D3D12_SAMPLER_DESC from SamplerConfig
        D3D12_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter             = m_config.filter;
        samplerDesc.AddressU           = m_config.addressU;
        samplerDesc.AddressV           = m_config.addressV;
        samplerDesc.AddressW           = m_config.addressW;
        samplerDesc.MipLODBias         = m_config.mipLODBias;
        samplerDesc.MaxAnisotropy      = m_config.maxAnisotropy;
        samplerDesc.ComparisonFunc     = m_config.comparisonFunc;
        samplerDesc.BorderColor[0]     = m_config.borderColor[0];
        samplerDesc.BorderColor[1]     = m_config.borderColor[1];
        samplerDesc.BorderColor[2]     = m_config.borderColor[2];
        samplerDesc.BorderColor[3]     = m_config.borderColor[3];
        samplerDesc.MinLOD             = m_config.minLOD;
        samplerDesc.MaxLOD             = m_config.maxLOD;

        // Create sampler at allocated CPU handle
        device->CreateSampler(&samplerDesc, m_allocation.cpuHandle);
    }

    void Sampler::Release()
    {
        if (m_heapManager && m_allocation.isValid)
        {
            m_heapManager->FreeSampler(m_allocation);
        }

        m_heapManager = nullptr;
        m_allocation.Reset();
        m_bindlessIndex = INVALID_SAMPLER_INDEX;
    }
} // namespace enigma::graphic
