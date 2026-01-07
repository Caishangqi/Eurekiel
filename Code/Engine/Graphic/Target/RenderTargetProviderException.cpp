#include "RenderTargetProviderException.hpp"
using namespace enigma::graphic;

RenderTargetProviderException::RenderTargetProviderException(const std::string& message) : std::runtime_error(message)
{
}

InvalidIndexException::InvalidIndexException(const std::string& providerName, int index, int maxIndex) : RenderTargetProviderException(
    providerName + ":: Invalid index " + std::to_string(index) + ", valid range [0, " + std::to_string(maxIndex - 1) + "]")
{
}

ResourceNotReadyException::ResourceNotReadyException(const std::string& resourceName)
    : RenderTargetProviderException("Resource not ready: " + resourceName)
{
}

CopyOperationFailedException::CopyOperationFailedException(const std::string& providerName, int srcIndex, int dstIndex)
    : RenderTargetProviderException(
        providerName + ":: Copy failed from " + std::to_string(srcIndex) +
        " to " + std::to_string(dstIndex))
{
}

InvalidBindingException::InvalidBindingException(Reason reason)
    : RenderTargetProviderException(GetMessage(reason))
      , m_reason(reason)
{
}

std::string InvalidBindingException::GetMessage(Reason reason)
{
    switch (reason)
    {
    case Reason::DualDepthBinding:
        return "RenderTargetBinder:: Cannot bind both ShadowTex and DepthTex - "
            "DirectX 12 only allows one depth buffer per pass";
    case Reason::MultipleShadowTex:
        return "RenderTargetBinder:: Cannot bind multiple ShadowTex - "
            "only one shadow depth texture allowed per pass";
    case Reason::MultipleDepthTex:
        return "RenderTargetBinder:: Cannot bind multiple DepthTex - "
            "only one depth texture allowed per pass";
    default:
        return "RenderTargetBinder:: Invalid binding configuration";
    }
}
