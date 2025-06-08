#include "Shader.hpp"
#include "Renderer.hpp"

Shader::Shader(ShaderConfig& config): m_config(config)
{
}

Shader::~Shader()
{
    DX_SAFE_RELEASE(m_vertexShader)
    DX_SAFE_RELEASE(m_pixelShader)
    DX_SAFE_RELEASE(m_inputLayout)

    DX_SAFE_RELEASE(m_vsBytecode)
    DX_SAFE_RELEASE(m_psBytecode)
}

const std::string& Shader::GetName() const
{
    return m_config.m_name;
}
