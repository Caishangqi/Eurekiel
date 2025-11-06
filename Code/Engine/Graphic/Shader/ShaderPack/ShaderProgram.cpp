/**
 * @file ShaderProgram.cpp
 * @brief 着色器程序实现
 * @date 2025-10-03
 */

#include "ShaderProgram.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"

namespace enigma::graphic
{
    // ========================================================================
    // 构造和析构
    // ========================================================================

    ShaderProgram::~ShaderProgram()
    {
        // Root Signature 由 D3D12RenderSystem 持有，不需要释放
        // CompiledShader 会自动释放
    }

    // ========================================================================
    // 创建方法
    // ========================================================================

    bool ShaderProgram::Create(
        CompiledShader&&                 vertexShader,
        CompiledShader&&                 pixelShader,
        std::optional<CompiledShader>&&  geometryShader,
        ShaderType                       type,
        const shader::ProgramDirectives& directives
    )
    {
        // 1. 存储着色器
        m_vertexShader   = std::move(vertexShader);
        m_pixelShader    = std::move(pixelShader);
        m_geometryShader = std::move(geometryShader);
        m_type           = type;

        // 2. 设置程序名称
        m_name = m_vertexShader.name;

        // 3. 存储 ProgramDirectives (从 ShaderSource 传入)
        m_directives = directives;

        // 4. 获取全局 Bindless Root Signature
        ID3D12RootSignature* rootSig = D3D12RenderSystem::GetBindlessRootSignature();
        if (!rootSig)
        {
            ERROR_AND_DIE("Failed to get Bindless Root Signature")
        }
        m_rootSignature = rootSig;

        return true;
    }

    // ========================================================================
    // 使用方法
    // ========================================================================

    void ShaderProgram::Use(ID3D12GraphicsCommandList* commandList)
    {
        if (!IsValid())
        {
            ERROR_AND_DIE("Attempting to use invalid shader program");
            return;
        }

        // 设置 Root Signature (PSO由PSOManager管理)
        commandList->SetGraphicsRootSignature(m_rootSignature);
    }

    void ShaderProgram::Unbind(ID3D12GraphicsCommandList* commandList)
    {
        UNUSED(commandList)
        // DirectX 12 中不需要显式解绑 PSO
        // 保留此方法是为了与 Iris 架构一致
        //
        // 对应 Iris:
        // public static void unbind() {
        //     ProgramManager.glUseProgram(0);
        // }
    }
} // namespace enigma::graphic
