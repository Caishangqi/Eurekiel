#pragma once

#include <array>
#include <cstdint>
#include <d3d12.h>

#include "Engine/Graphic/Resource/BindlessRootSignature.hpp"

namespace enigma::graphic
{
    struct GraphicsRootBindingCache
    {
        static constexpr uint32_t ENGINE_ROOT_CBV_SLOT_COUNT = BindlessRootSignature::ROOT_DESCRIPTOR_TABLE_CUSTOM;

        ID3D12RootSignature* rootSignature = nullptr;
        std::array<D3D12_GPU_VIRTUAL_ADDRESS, ENGINE_ROOT_CBV_SLOT_COUNT> engineCbv = {};
        std::array<bool, ENGINE_ROOT_CBV_SLOT_COUNT>                      engineCbvValid = {};
        D3D12_GPU_DESCRIPTOR_HANDLE                                       customTable = {};
        bool                                                              customTableValid = false;

        void InvalidateAll()
        {
            rootSignature = nullptr;
            engineCbv.fill(0);
            engineCbvValid.fill(false);
            customTable = {};
            customTableValid = false;
        }

        void InvalidateDescriptorTables()
        {
            customTable = {};
            customTableValid = false;
        }

        void InvalidateEngineCbv(uint32_t rootParameterIndex)
        {
            if (rootParameterIndex >= ENGINE_ROOT_CBV_SLOT_COUNT)
            {
                return;
            }

            engineCbv[rootParameterIndex]      = 0;
            engineCbvValid[rootParameterIndex] = false;
        }
    };

    struct GraphicsRootBindingDiagnostics
    {
        uint64_t rootSignatureBindCount = 0;
        uint64_t rootSignatureCacheHitCount = 0;
        uint64_t engineCbvBindCount = 0;
        uint64_t engineCbvCacheHitCount = 0;
        uint64_t descriptorTableBindCount = 0;
        uint64_t descriptorTableCacheHitCount = 0;
        uint64_t fullInvalidationCount = 0;
        uint64_t descriptorTableInvalidationCount = 0;

        void Reset()
        {
            *this = {};
        }
    };
} // namespace enigma::graphic
