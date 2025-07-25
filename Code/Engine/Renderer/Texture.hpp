#pragma once
#include <d3d11.h>
#include <d3d12.h>
#include <string>

#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec2.hpp"


class Texture
{
    friend class Renderer; // Only the Renderer can create new Texture objects!
    friend class DX12Renderer;
    friend class DX11Renderer;

    // TODO: In the future, the resource subsystem will owned the textures.
    Texture(); // can't instantiate directly; must ask Renderer to do it for you
    Texture(const Texture& copy) = delete; // No copying allowed!  This represents GPU memory.

public:
    ~Texture();

public:
    IntVec2            GetDimensions() const { return m_dimensions; }
    Vec2               GetStandardDimensions() const;
    const std::string& GetImageFilePath() const { return m_name; }
    static UINT        IncrementInternalID() { return ++s_internalID; }

    ID3D11ShaderResourceView* GetShaderResourceView() const { return m_shaderResourceView; }

protected:
    std::string m_name;
    IntVec2     m_dimensions;

    static UINT s_internalID;

    /// OpenGL
    // #ToDo in SD2: Use #if defined( ENGINE_RENDER_D3D11 ) to do something different for DX11; #else do:
    unsigned int m_openglTextureID = 0xFFFFFFFF;
    ///

    /// DirectX11
    ID3D11Texture2D*          m_texture            = nullptr;
    ID3D11ShaderResourceView* m_shaderResourceView = nullptr;

    /// DirectX12
    ID3D12Resource*             m_dx12Texture               = nullptr;
    ID3D12Resource*             m_textureBufferUploadHeap   = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuShaderSourceViewHandle = {0};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuShaderSourceViewHandle = {0};
    ///
};
