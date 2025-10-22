#include "D12VertexBuffer.hpp"
#include <cassert>
#include <sstream>

using namespace enigma::graphic;

// ============================================================================
// 构造函数和析构函数
// ============================================================================

D12VertexBuffer::D12VertexBuffer(size_t size, size_t stride, const void* initialData, const char* debugName)
    : D12Buffer([&]()
      {
          BufferCreateInfo info;
          info.size  = size;
          info.usage = BufferUsage::VertexBuffer;
          // 教学要点: 应用层VertexBuffer需要频繁更新，统一使用CPUWritable
          // 即使没有initialData，也可能在后续通过UpdateBuffer更新
          // 参考Iris: 动态顶点数据使用UPLOAD heap (D3D12_HEAP_TYPE_UPLOAD)
          info.memoryAccess = MemoryAccess::CPUWritable;
          info.initialData  = initialData;
          info.debugName    = debugName;
          return info;
      }())
      , m_stride(stride)
      , m_view{}
{
    // 教学要点: 参数验证 - 确保输入合法性
    assert(stride > 0 && "Vertex stride must be greater than 0");
    assert(size % stride == 0 && "Buffer size must be multiple of stride");

    // 教学要点: 创建D3D12_VERTEX_BUFFER_VIEW
    // 这是DirectX 12绑定顶点缓冲区的关键结构
    UpdateView();
}

// ============================================================================
// VertexBufferView管理
// ============================================================================

void D12VertexBuffer::UpdateView()
{
    // 教学要点: 获取基类的资源指针
    auto* resource = GetResource();
    if (!resource)
    {
        // 资源无效时清空view
        m_view = {};
        return;
    }

    // 教学要点: 构造D3D12_VERTEX_BUFFER_VIEW
    // 1. BufferLocation: GPU虚拟地址（从ID3D12Resource获取）
    // 2. SizeInBytes: 缓冲区总大小
    // 3. StrideInBytes: 单个顶点大小
    m_view.BufferLocation = resource->GetGPUVirtualAddress();
    m_view.SizeInBytes    = static_cast<UINT>(GetSize());
    m_view.StrideInBytes  = static_cast<UINT>(m_stride);
}

// ============================================================================
// Debug接口实现
// ============================================================================

void D12VertexBuffer::SetDebugName(const std::string& name)
{
    // 教学要点: 调用基类实现，设置DirectX对象名称
    D12Buffer::SetDebugName(name);
}

std::string D12VertexBuffer::GetDebugInfo() const
{
    // 教学要点: 提供顶点缓冲区特有的调试信息
    std::ostringstream oss;
    oss << "VertexBuffer [" << GetDebugName() << "]\n";
    oss << "  Size: " << GetSize() << " bytes\n";
    oss << "  Stride: " << m_stride << " bytes\n";
    oss << "  Vertex Count: " << GetVertexCount() << "\n";
    oss << "  GPU Address: 0x" << std::hex << m_view.BufferLocation << std::dec << "\n";
    oss << "  Valid: " << (IsValid() ? "Yes" : "No");
    return oss.str();
}

// ============================================================================
// 资源释放
// ============================================================================

void D12VertexBuffer::ReleaseResource()
{
    // 教学要点: 清理VertexBufferView
    m_view = {};

    // 调用基类实现，释放DirectX资源
    D12Buffer::ReleaseResource();
}
