#pragma once

#include <memory>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace enigma::graphic {

/**
 * @brief D12Resource类 - DirectX 12资源的基础封装
 * 
 * 教学要点:
 * 1. 这是所有DX12资源的基类，封装ID3D12Resource的公共操作
 * 2. 使用RAII管理资源生命周期，确保正确释放GPU内存
 * 3. 支持资源状态追踪，这是DX12的重要特性
 * 4. 专门为Bindless架构和延迟渲染设计
 * 
 * DirectX 12特性:
 * - 显式资源状态管理 (不同于DX11的隐式状态)
 * - 支持资源别名和子资源
 * - GPU虚拟地址访问 (用于Bindless绑定)
 * 
 * @note 这是新渲染系统专用的资源封装，不复用旧的设计
 */
class D12Resource {
protected:
    ID3D12Resource*         m_resource;         // DX12资源指针
    D3D12_RESOURCE_STATES   m_currentState;     // 当前资源状态
    std::string             m_debugName;        // 调试名称
    size_t                  m_size;             // 资源大小 (字节)
    bool                    m_isValid;          // 资源有效性标记
    
public:
    /**
     * @brief 构造函数
     * 
     * 教学要点: 基类构造函数初始化通用属性
     */
    D12Resource();
    
    /**
     * @brief 虚析构函数
     * 
     * 教学要点: 虚析构函数确保派生类正确释放资源
     */
    virtual ~D12Resource();
    
    // ========================================================================
    // 资源访问接口
    // ========================================================================
    
    /**
     * @brief 获取DX12资源指针
     * @return ID3D12Resource指针
     */
    ID3D12Resource* GetResource() const { return m_resource; }
    
    /**
     * @brief 获取GPU虚拟地址 (用于Bindless绑定)
     * @return GPU虚拟地址
     * 
     * 教学要点: GPU虚拟地址是DX12 Bindless资源绑定的关键
     */
    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;
    
    /**
     * @brief 获取当前资源状态
     * @return 资源状态枚举
     */
    D3D12_RESOURCE_STATES GetCurrentState() const { return m_currentState; }
    
    /**
     * @brief 设置资源状态 (通常由资源状态追踪器调用)
     * @param newState 新的资源状态
     */
    void SetCurrentState(D3D12_RESOURCE_STATES newState) { m_currentState = newState; }
    
    /**
     * @brief 获取资源大小
     * @return 资源大小 (字节)
     */
    size_t GetSize() const { return m_size; }
    
    /**
     * @brief 检查资源是否有效
     * @return 有效返回true，否则返回false
     */
    bool IsValid() const { return m_isValid && m_resource != nullptr; }
    
    /**
     * @brief 设置调试名称 (用于PIX调试)
     * @param name 调试名称
     */
    void SetDebugName(const std::string& name);
    
    /**
     * @brief 获取调试名称
     * @return 调试名称字符串
     */
    const std::string& GetDebugName() const { return m_debugName; }

protected:
    /**
     * @brief 设置资源指针 (由派生类调用)
     * @param resource DX12资源指针
     * @param initialState 初始资源状态
     * @param size 资源大小
     */
    void SetResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, size_t size);
    
    /**
     * @brief 释放资源
     * 
     * 教学要点: 安全释放DX12资源，避免重复释放
     */
    virtual void ReleaseResource();

private:
    // 禁用拷贝构造和赋值
    D12Resource(const D12Resource&) = delete;
    D12Resource& operator=(const D12Resource&) = delete;
    
    // 支持移动语义
    D12Resource(D12Resource&&) = default;
    D12Resource& operator=(D12Resource&&) = default;
};



} // namespace enigma::graphic