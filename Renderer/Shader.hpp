#pragma once
#include <d3d11.h>
#include <d3d12.h>
#include <string>
#include <vector>

struct D3D12_INPUT_ELEMENT_DESC;

struct ShaderConfig
{
    std::string m_name;
    std::string m_vertexEntryPoint = "VertexMain";
    std::string m_pixelEntryPoint  = "PixelMain";
};

class Shader
{
    friend class Renderer;
    friend class DX12Renderer;

public:
    Shader(ShaderConfig& config);
    Shader(const Shader& copy) = delete;
    ~Shader();

    const std::string& GetName() const;

private:
    ShaderConfig        m_config;
    ID3D11VertexShader* m_vertexShader = nullptr;
    ID3D11PixelShader*  m_pixelShader  = nullptr;
    ID3D11InputLayout*  m_inputLayout  = nullptr;

    // DirectX 12
    ID3DBlob* m_vertexShaderBlob = nullptr; // VS / PS 字节码 – 供 PSO 复用
    ID3DBlob* m_pixelShaderBlob  = nullptr;

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_dx12InputLayout = {};
};
