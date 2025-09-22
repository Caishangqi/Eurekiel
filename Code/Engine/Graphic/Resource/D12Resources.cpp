#include "D12Resources.hpp"
#include <cassert>

namespace enigma::graphic
{
    // ===== D12Resource基类实现 =====

    /**
     * 构造函数实现
     * 教学要点：基类构造函数初始化通用属性
     */
    D12Resource::D12Resource()
        : m_resource(nullptr)
          , m_currentState(D3D12_RESOURCE_STATE_COMMON)
          , m_debugName()
          , m_size(0)
          , m_isValid(false)
    {
        // 基类构造函数只进行基本初始化
        // 实际资源创建由派生类负责
    }

    /**
     * 虚析构函数实现
     * 教学要点：虚析构函数确保派生类正确释放资源
     */
    D12Resource::~D12Resource()
    {
        // 释放DirectX 12资源
        D12Resource::ReleaseResource();
    }

    /**
     * 获取GPU虚拟地址实现
     * DirectX 12 API: ID3D12Resource::GetGPUVirtualAddress()
     * 教学要点：GPU虚拟地址是DX12 Bindless资源绑定的关键
     */
    D3D12_GPU_VIRTUAL_ADDRESS D12Resource::GetGPUVirtualAddress() const
    {
        if (!m_resource)
        {
            return 0;
        }

        // DirectX 12 API: ID3D12Resource::GetGPUVirtualAddress()
        // 返回资源在GPU虚拟地址空间中的地址
        // 用于Bindless描述符绑定和Shader Resource View
        return m_resource->GetGPUVirtualAddress();
    }

    /**
     * 设置调试名称实现
     * DirectX 12 API: ID3D12Object::SetName()
     * 教学要点：调试名称对PIX调试和性能分析非常重要
     */
    void D12Resource::SetDebugName(const std::string& name)
    {
        m_debugName = name;

        if (m_resource && !name.empty())
        {
            // 转换为宽字符串
            size_t   len      = name.length();
            wchar_t* wideName = new wchar_t[len + 1];

            size_t convertedChars = 0;
            mbstowcs_s(&convertedChars, wideName, len + 1, name.c_str(), len);

            // DirectX 12 API: ID3D12Object::SetName()
            // 设置DirectX对象的调试名称，便于PIX调试
            m_resource->SetName(wideName);

            delete[] wideName;
        }
    }

    /**
     * 设置资源指针（由派生类调用）
     * 教学要点：统一的资源设置接口，确保状态一致性
     */
    void D12Resource::SetResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, size_t size)
    {
        // 释放之前的资源
        ReleaseResource();

        // 设置新资源
        m_resource     = resource;
        m_currentState = initialState;
        m_size         = size;
        m_isValid      = (resource != nullptr);

        // 如果有调试名称，应用到新资源
        if (m_resource && !m_debugName.empty())
        {
            SetDebugName(m_debugName);
        }
    }

    /**
     * 释放资源实现
     * 教学要点：安全释放DX12资源，避免重复释放
     */
    void D12Resource::ReleaseResource()
    {
        if (m_resource)
        {
            // DirectX 12资源使用COM接口
            // Release()会减少引用计数，当计数为0时自动销毁
            m_resource->Release();
            m_resource = nullptr;
        }

        // 重置状态
        m_currentState = D3D12_RESOURCE_STATE_COMMON;
        m_size         = 0;
        m_isValid      = false;
    }
} // namespace enigma::graphic
