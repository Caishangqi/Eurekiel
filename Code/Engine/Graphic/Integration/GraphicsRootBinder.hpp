#pragma once

#include <cstdint>
#include <d3d12.h>

#include "Engine/Graphic/Integration/GraphicsRootBindingCache.hpp"

namespace enigma::graphic
{
    class GraphicsRootBinder final
    {
    public:
        GraphicsRootBinder();
        ~GraphicsRootBinder() = default;

        GraphicsRootBinder(const GraphicsRootBinder&) = delete;
        GraphicsRootBinder& operator=(const GraphicsRootBinder&) = delete;
        GraphicsRootBinder(GraphicsRootBinder&&) = delete;
        GraphicsRootBinder& operator=(GraphicsRootBinder&&) = delete;

        bool BindRootSignatureIfDirty(
            ID3D12GraphicsCommandList* commandList,
            ID3D12RootSignature*       rootSignature
        );

        bool BindEngineCbvIfDirty(
            ID3D12GraphicsCommandList*  commandList,
            uint32_t                    rootParameterIndex,
            D3D12_GPU_VIRTUAL_ADDRESS   gpuVirtualAddress
        );

        bool BindDescriptorTableIfDirty(
            ID3D12GraphicsCommandList*   commandList,
            uint32_t                     rootParameterIndex,
            D3D12_GPU_DESCRIPTOR_HANDLE  descriptorHandle
        );

        void InvalidateAll();
        void InvalidateDescriptorTables();
        void ResetDiagnostics();

        const GraphicsRootBindingCache& GetCache() const { return m_cache; }
        const GraphicsRootBindingDiagnostics& GetDiagnostics() const { return m_diagnostics; }

    private:
        bool ValidateCommandList(ID3D12GraphicsCommandList* commandList, const char* methodName) const;
        bool ValidateEngineRootParameter(uint32_t rootParameterIndex) const;
        bool ValidateDescriptorTableRootParameter(uint32_t rootParameterIndex) const;

    private:
        GraphicsRootBindingCache       m_cache;
        GraphicsRootBindingDiagnostics m_diagnostics;
    };
} // namespace enigma::graphic
