#include "Engine/Graphic/Integration/GraphicsRootBinder.hpp"

#include <string>

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"

namespace enigma::graphic
{
    GraphicsRootBinder::GraphicsRootBinder()
    {
        m_cache.InvalidateAll();
    }

    bool GraphicsRootBinder::BindRootSignatureIfDirty(
        ID3D12GraphicsCommandList* commandList,
        ID3D12RootSignature*       rootSignature
    )
    {
        if (!ValidateCommandList(commandList, "BindRootSignatureIfDirty"))
        {
            return false;
        }

        if (rootSignature == nullptr)
        {
            ERROR_RECOVERABLE("GraphicsRootBinder::BindRootSignatureIfDirty received a null root signature");
            m_cache.InvalidateAll();
            return false;
        }

        if (m_cache.rootSignature == rootSignature)
        {
            ++m_diagnostics.rootSignatureCacheHitCount;
            return false;
        }

        commandList->SetGraphicsRootSignature(rootSignature);

        m_cache.InvalidateAll();
        m_cache.rootSignature = rootSignature;
        ++m_diagnostics.rootSignatureBindCount;
        return true;
    }

    bool GraphicsRootBinder::BindEngineCbvIfDirty(
        ID3D12GraphicsCommandList* commandList,
        uint32_t                  rootParameterIndex,
        D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress
    )
    {
        if (!ValidateCommandList(commandList, "BindEngineCbvIfDirty"))
        {
            return false;
        }

        if (!ValidateEngineRootParameter(rootParameterIndex))
        {
            ERROR_RECOVERABLE(std::string("GraphicsRootBinder::BindEngineCbvIfDirty received invalid root parameter index: ")
                + std::to_string(rootParameterIndex));
            return false;
        }

        if (gpuVirtualAddress == 0)
        {
            m_cache.InvalidateEngineCbv(rootParameterIndex);
            return false;
        }

        if (m_cache.rootSignature == nullptr)
        {
            ERROR_RECOVERABLE("GraphicsRootBinder::BindEngineCbvIfDirty called before any graphics root signature was cached");
        }

        if (m_cache.engineCbvValid[rootParameterIndex] && m_cache.engineCbv[rootParameterIndex] == gpuVirtualAddress)
        {
            ++m_diagnostics.engineCbvCacheHitCount;
            return false;
        }

        commandList->SetGraphicsRootConstantBufferView(rootParameterIndex, gpuVirtualAddress);
        m_cache.engineCbv[rootParameterIndex]      = gpuVirtualAddress;
        m_cache.engineCbvValid[rootParameterIndex] = true;
        ++m_diagnostics.engineCbvBindCount;
        return true;
    }

    bool GraphicsRootBinder::BindDescriptorTableIfDirty(
        ID3D12GraphicsCommandList*  commandList,
        uint32_t                    rootParameterIndex,
        D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle
    )
    {
        if (!ValidateCommandList(commandList, "BindDescriptorTableIfDirty"))
        {
            return false;
        }

        if (!ValidateDescriptorTableRootParameter(rootParameterIndex))
        {
            ERROR_RECOVERABLE(std::string("GraphicsRootBinder::BindDescriptorTableIfDirty received invalid descriptor-table root parameter index: ")
                + std::to_string(rootParameterIndex));
            return false;
        }

        if (descriptorHandle.ptr == 0)
        {
            m_cache.InvalidateDescriptorTables();
            return false;
        }

        if (m_cache.rootSignature == nullptr)
        {
            ERROR_RECOVERABLE("GraphicsRootBinder::BindDescriptorTableIfDirty called before any graphics root signature was cached");
        }

        if (m_cache.customTableValid && m_cache.customTable.ptr == descriptorHandle.ptr)
        {
            ++m_diagnostics.descriptorTableCacheHitCount;
            return false;
        }

        commandList->SetGraphicsRootDescriptorTable(rootParameterIndex, descriptorHandle);
        m_cache.customTable      = descriptorHandle;
        m_cache.customTableValid = true;
        ++m_diagnostics.descriptorTableBindCount;
        return true;
    }

    void GraphicsRootBinder::InvalidateAll()
    {
        m_cache.InvalidateAll();
        ++m_diagnostics.fullInvalidationCount;
    }

    void GraphicsRootBinder::InvalidateDescriptorTables()
    {
        m_cache.InvalidateDescriptorTables();
        ++m_diagnostics.descriptorTableInvalidationCount;
    }

    void GraphicsRootBinder::ResetDiagnostics()
    {
        m_diagnostics.Reset();
    }

    bool GraphicsRootBinder::ValidateCommandList(ID3D12GraphicsCommandList* commandList, const char* methodName) const
    {
        if (commandList != nullptr)
        {
            return true;
        }

        ERROR_RECOVERABLE(std::string("GraphicsRootBinder::") + methodName + " requires a valid command list");
        return false;
    }

    bool GraphicsRootBinder::ValidateEngineRootParameter(uint32_t rootParameterIndex) const
    {
        return rootParameterIndex < GraphicsRootBindingCache::ENGINE_ROOT_CBV_SLOT_COUNT;
    }

    bool GraphicsRootBinder::ValidateDescriptorTableRootParameter(uint32_t rootParameterIndex) const
    {
        return rootParameterIndex == BindlessRootSignature::ROOT_DESCRIPTOR_TABLE_CUSTOM;
    }
} // namespace enigma::graphic
