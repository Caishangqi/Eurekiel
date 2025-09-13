#pragma once

#include <memory>
#include <vector>
#include <array>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>
#include "Resource/D12Resources.hpp"

namespace enigma::graphic {

// 前向声明
class BindlessResourceManager;

/**
 * @brief GBuffer类 - 延迟渲染几何缓冲区管理器
 * 
 * 教学要点:
 * 1. 延迟渲染的核心概念：将几何渲染和光照计算分离
 * 2. G-Buffer存储几何体的材质属性，而不是最终颜色
 * 3. 使用Multiple Render Targets (MRT) 同时写入多个RT
 * 4. 每个RT存储不同类型的数据，优化内存访问模式
 * 
 * Iris兼容性:
 * - RT0-RT3: 主要G-Buffer，存储几何属性
 * - RT4-RT9: 临时缓冲区，用于后处理链
 * - 支持运行时格式配置和分辨率缩放
 * - Ping-Pong缓冲机制，避免读写冲突
 * 
 * DirectX 12特性:
 * - 使用Bindless资源绑定避免频繁的描述符更新
 * - 通过资源状态转换管理RT的读写权限
 * - 支持可配置的RT格式和分辨率
 * - 集成MSAA抗锯齿支持
 * 
 * @note 这是教学项目，重点理解G-Buffer的设计原理和数据布局
 */
class GBuffer final {
public:
    /**
     * @brief G-Buffer目标枚举 - 对应Iris规范
     * 
     * 教学要点: 明确每个RT的用途，便于理解延迟渲染的数据流
     */
    enum class Target : uint32_t {
        // 主要G-Buffer (RT0-RT3) - 存储几何属性
        Albedo = 0,         // RT0: Albedo.rgb + MaterialID.a (RGBA8)
        Normal = 1,         // RT1: Normal.rgb + Roughness.a (RGBA8)
        Material = 2,       // RT2: Metallic.r + AO.g + Emission.ba (RGBA8)
        Motion = 3,         // RT3: Motion Vector.rg + Depth.ba (RG16F + D32F)
        
        // 临时缓冲区 (RT4-RT9) - 用于后处理
        Composite0 = 4,     // RT4: 第一个临时缓冲 (RGBA16F)
        Composite1 = 5,     // RT5: 第二个临时缓冲 (RGBA16F)
        Composite2 = 6,     // RT6: 第三个临时缓冲 (RGBA16F)
        Composite3 = 7,     // RT7: 第四个临时缓冲 (RGBA16F)
        Composite4 = 8,     // RT8: 第五个临时缓冲 (可选, RGBA16F)
        Composite5 = 9,     // RT9: 第六个临时缓冲 (可选, RGBA16F)
        
        Count = 10          // 总RT数量
    };
    
    /**
     * @brief 缓冲区类别枚举
     */
    enum class Category {
        MainGBuffer,        // 主要G-Buffer (RT0-RT3)
        CompositeBuffer,    // 临时后处理缓冲 (RT4-RT9)
        DepthStencil       // 深度模板缓冲
    };

private:
    /**
     * @brief RT配置信息结构
     * 
     * 教学要点: 封装每个RT的配置参数，便于统一管理
     */
    struct RTConfig {
        DXGI_FORMAT         format;            // 像素格式
        D3D12_CLEAR_VALUE   clearValue;        // 清除值 (用于优化)
        float               resolutionScale;   // 分辨率缩放因子
        bool                enabled;           // 是否启用
        std::string         debugName;         // 调试名称
        
        RTConfig();
        RTConfig(DXGI_FORMAT fmt, const D3D12_CLEAR_VALUE& clear, 
                float scale = 1.0f, bool enable = true, const std::string& name = "");
    };
    
    // 核心资源
    ID3D12Device*           m_device;           // DX12设备 (外部管理)
    BindlessResourceManager* m_bindlessManager; // Bindless资源管理器 (外部管理)
    
    // RT资源管理
    std::array<std::shared_ptr<D12Texture>, static_cast<size_t>(Target::Count)> m_renderTargets;
    std::shared_ptr<D12Texture> m_depthStencil; // 主深度模板缓冲
    std::array<RTConfig, static_cast<size_t>(Target::Count)> m_rtConfigs; // RT配置
    
    // Bindless索引 - 用于着色器访问
    std::array<uint32_t, static_cast<size_t>(Target::Count)> m_bindlessIndices;
    uint32_t                m_depthBindlessIndex;   // 深度缓冲的Bindless索引
    
    // 渲染配置
    uint32_t                m_baseWidth;        // 基础渲染宽度
    uint32_t                m_baseHeight;       // 基础渲染高度
    uint32_t                m_sampleCount;      // MSAA采样数量
    bool                    m_enableMSAA;       // 是否启用MSAA
    
    // 状态管理
    bool                    m_initialized;      // 初始化状态
    bool                    m_resourcesCreated; // 资源创建状态
    
    // 默认配置
    static constexpr uint32_t DEFAULT_WIDTH = 1920;
    static constexpr uint32_t DEFAULT_HEIGHT = 1080;
    static constexpr uint32_t DEFAULT_SAMPLE_COUNT = 1;

public:
    /**
     * @brief 构造函数
     * 
     * 教学要点: 初始化默认配置，实际资源创建在Initialize中
     */
    GBuffer();
    
    /**
     * @brief 析构函数
     * 
     * 教学要点: 智能指针自动释放资源，但需要从Bindless管理器注销
     */
    ~GBuffer();
    
    // ========================================================================
    // 生命周期管理
    // ========================================================================
    
    /**
     * @brief 初始化G-Buffer系统
     * @param device DX12设备指针
     * @param bindlessManager Bindless资源管理器
     * @return 成功返回true，失败返回false
     * 
     * 教学要点: 设置默认的Iris兼容RT配置
     */
    bool Initialize(ID3D12Device* device, BindlessResourceManager* bindlessManager);
    
    /**
     * @brief 创建G-Buffer资源
     * @param width 渲染宽度
     * @param height 渲染高度
     * @param sampleCount MSAA采样数量 (1表示无MSAA)
     * @return 成功返回true，失败返回false
     * 
     * 教学要点:
     * 1. 根据配置创建所有RT和深度缓冲
     * 2. 注册到Bindless管理器获取全局索引
     * 3. 设置调试名称便于PIX调试
     */
    bool CreateResources(uint32_t width, uint32_t height, uint32_t sampleCount = DEFAULT_SAMPLE_COUNT);
    
    /**
     * @brief 释放所有G-Buffer资源
     * 
     * 教学要点: 从Bindless管理器注销，然后释放智能指针
     */
    void ReleaseResources();
    
    /**
     * @brief 重新创建资源 (分辨率变更时使用)
     * @param newWidth 新宽度
     * @param newHeight 新高度
     * @param newSampleCount 新采样数量
     * @return 成功返回true，失败返回false
     */
    bool RecreateResources(uint32_t newWidth, uint32_t newHeight, uint32_t newSampleCount = 0);
    
    // ========================================================================
    // RT配置管理
    // ========================================================================
    
    /**
     * @brief 配置RT格式和参数
     * @param target RT目标
     * @param format 像素格式
     * @param clearColor 清除颜色 (RGBA)
     * @param resolutionScale 分辨率缩放 (1.0=全分辨率, 0.5=半分辨率)
     * @param debugName 调试名称
     * 
     * 教学要点: 允许运行时配置RT参数，支持不同的渲染质量设置
     */
    void ConfigureRT(Target target, DXGI_FORMAT format, 
                    const float clearColor[4], 
                    float resolutionScale = 1.0f,
                    const std::string& debugName = "");
    
    /**
     * @brief 启用/禁用指定RT
     * @param target RT目标
     * @param enabled 是否启用
     * 
     * 教学要点: 可以禁用不需要的RT以节省显存
     */
    void SetRTEnabled(Target target, bool enabled);
    
    /**
     * @brief 应用Iris标准配置
     * 
     * 教学要点: 设置符合Iris规范的默认RT配置
     * - RT0: Albedo + MaterialID (RGBA8)
     * - RT1: Normal + Roughness (RGBA8)
     * - RT2: Material Properties (RGBA8)
     * - RT3: Motion Vector + Depth (RG16F)
     * - RT4-RT9: 后处理临时缓冲 (RGBA16F)
     */
    void ApplyIrisStandardConfig();
    
    /**
     * @brief 应用高质量配置 (更高精度格式)
     * 
     * 教学要点: 使用更高精度的格式以获得更好的视觉质量
     */
    void ApplyHighQualityConfig();
    
    // ========================================================================
    // 渲染接口
    // ========================================================================
    
    /**
     * @brief 清除所有G-Buffer RT
     * @param commandList 命令列表
     * 
     * 教学要点: 在每帧开始时清除所有RT为默认值
     */
    void ClearAllRTs(ID3D12GraphicsCommandList* commandList);
    
    /**
     * @brief 清除指定的RT
     * @param commandList 命令列表
     * @param target 要清除的RT
     */
    void ClearRT(ID3D12GraphicsCommandList* commandList, Target target);
    
    /**
     * @brief 清除深度缓冲
     * @param commandList 命令列表
     * @param clearDepth 清除深度值 (通常为1.0f)
     * @param clearStencil 清除模板值 (通常为0)
     */
    void ClearDepthStencil(ID3D12GraphicsCommandList* commandList, 
                          float clearDepth = 1.0f, uint8_t clearStencil = 0);
    
    /**
     * @brief 设置G-Buffer作为渲染目标 (用于几何渲染阶段)
     * @param commandList 命令列表
     * @param targets 要使用的RT目标数组
     * @param targetCount RT数量 (最多8个)
     * @param useDepth 是否使用深度缓冲
     * 
     * 教学要点: 设置MRT，同时写入多个G-Buffer RT
     */
    void SetAsRenderTargets(ID3D12GraphicsCommandList* commandList, 
                           const Target* targets, uint32_t targetCount, 
                           bool useDepth = true);
    
    /**
     * @brief 设置主要G-Buffer作为渲染目标 (RT0-RT3)
     * @param commandList 命令列表
     * @param useDepth 是否使用深度缓冲
     * 
     * 教学要点: 便捷方法，用于标准的几何渲染阶段
     */
    void SetMainGBufferAsRT(ID3D12GraphicsCommandList* commandList, bool useDepth = true);
    
    /**
     * @brief 转换RT为着色器资源状态 (用于读取)
     * @param commandList 命令列表
     * @param targets 要转换的RT数组
     * @param targetCount RT数量
     * 
     * 教学要点: 延迟渲染需要在写入后将RT转换为SRV状态供着色器读取
     */
    void TransitionToShaderResource(ID3D12GraphicsCommandList* commandList, 
                                   const Target* targets, uint32_t targetCount);
    
    /**
     * @brief 转换RT为渲染目标状态 (用于写入)
     * @param commandList 命令列表  
     * @param targets 要转换的RT数组
     * @param targetCount RT数量
     */
    void TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList, 
                                 const Target* targets, uint32_t targetCount);
    
    // ========================================================================
    // 资源访问接口
    // ========================================================================
    
    /**
     * @brief 获取指定RT的纹理对象
     * @param target RT目标
     * @return 纹理智能指针
     */
    std::shared_ptr<D12Texture> GetRenderTarget(Target target) const;
    
    /**
     * @brief 获取深度缓冲纹理对象
     * @return 深度缓冲智能指针
     */
    std::shared_ptr<D12Texture> GetDepthStencil() const { return m_depthStencil; }
    
    /**
     * @brief 获取RT的Bindless索引 (用于着色器访问)
     * @param target RT目标
     * @return Bindless资源索引
     * 
     * 教学要点: 着色器可以通过这个索引直接访问RT作为纹理
     */
    uint32_t GetBindlessIndex(Target target) const;
    
    /**
     * @brief 获取深度缓冲的Bindless索引
     * @return 深度缓冲的Bindless索引
     */
    uint32_t GetDepthBindlessIndex() const { return m_depthBindlessIndex; }
    
    /**
     * @brief 获取所有G-Buffer RT的RTV句柄数组 (用于MRT设置)
     * @param rtvHandles 输出RTV句柄数组
     * @param maxCount 数组最大容量
     * @return 实际填充的RTV数量
     */
    uint32_t GetAllRTVHandles(D3D12_CPU_DESCRIPTOR_HANDLE* rtvHandles, uint32_t maxCount) const;
    
    /**
     * @brief 获取深度缓冲的DSV句柄
     * @return DSV句柄
     */
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;
    
    // ========================================================================
    // 查询接口
    // ========================================================================
    
    /**
     * @brief 获取基础渲染分辨率
     * @param width 输出宽度
     * @param height 输出高度
     */
    void GetBaseResolution(uint32_t& width, uint32_t& height) const {
        width = m_baseWidth;
        height = m_baseHeight;
    }
    
    /**
     * @brief 获取实际RT分辨率 (考虑缩放因子)
     * @param target RT目标
     * @param width 输出宽度
     * @param height 输出高度
     */
    void GetRTResolution(Target target, uint32_t& width, uint32_t& height) const;
    
    /**
     * @brief 获取RT格式
     * @param target RT目标
     * @return 像素格式
     */
    DXGI_FORMAT GetRTFormat(Target target) const;
    
    /**
     * @brief 检查RT是否启用
     * @param target RT目标
     * @return 启用返回true，否则返回false
     */
    bool IsRTEnabled(Target target) const;
    
    /**
     * @brief 获取MSAA采样数量
     * @return 采样数量
     */
    uint32_t GetSampleCount() const { return m_sampleCount; }
    
    /**
     * @brief 检查是否启用MSAA
     * @return 启用返回true，否则返回false
     */
    bool IsMultisampleEnabled() const { return m_enableMSAA && m_sampleCount > 1; }
    
    /**
     * @brief 检查G-Buffer是否已初始化
     * @return 已初始化返回true，否则返回false
     */
    bool IsInitialized() const { return m_initialized; }
    
    /**
     * @brief 检查资源是否已创建
     * @return 已创建返回true，否则返回false
     */
    bool AreResourcesCreated() const { return m_resourcesCreated; }
    
    /**
     * @brief 估算G-Buffer占用的显存大小
     * @return 显存大小 (字节)
     * 
     * 教学要点: 帮助理解不同配置对显存消耗的影响
     */
    size_t EstimateMemoryUsage() const;

private:
    // ========================================================================
    // 私有辅助方法
    // ========================================================================
    
    /**
     * @brief 初始化默认RT配置
     * 
     * 教学要点: 设置Iris兼容的标准G-Buffer布局
     */
    void InitializeDefaultConfigs();
    
    /**
     * @brief 创建单个RT资源
     * @param target RT目标
     * @param width 宽度
     * @param height 高度
     * @param sampleCount 采样数量
     * @return 成功返回true，失败返回false
     */
    bool CreateRenderTarget(Target target, uint32_t width, uint32_t height, uint32_t sampleCount);
    
    /**
     * @brief 创建深度缓冲资源
     * @param width 宽度
     * @param height 高度  
     * @param sampleCount 采样数量
     * @return 成功返回true，失败返回false
     */
    bool CreateDepthStencil(uint32_t width, uint32_t height, uint32_t sampleCount);
    
    /**
     * @brief 注册资源到Bindless管理器
     * @return 成功返回true，失败返回false
     */
    bool RegisterToBindlessManager();
    
    /**
     * @brief 从Bindless管理器注销所有资源
     */
    void UnregisterFromBindlessManager();
    
    /**
     * @brief 获取RT类别
     * @param target RT目标
     * @return RT类别
     */
    static Category GetTargetCategory(Target target);
    
    /**
     * @brief 获取RT目标的字符串名称 (调试用)
     * @param target RT目标
     * @return 目标名称
     */
    static const char* GetTargetName(Target target);
    
    /**
     * @brief 计算RT的实际尺寸 (考虑缩放因子)
     * @param baseWidth 基础宽度
     * @param baseHeight 基础高度
     * @param scale 缩放因子
     * @param actualWidth 输出实际宽度
     * @param actualHeight 输出实际高度
     */
    static void CalculateScaledSize(uint32_t baseWidth, uint32_t baseHeight, float scale,
                                   uint32_t& actualWidth, uint32_t& actualHeight);
    
    // 禁用拷贝构造和赋值操作
    GBuffer(const GBuffer&) = delete;
    GBuffer& operator=(const GBuffer&) = delete;
    
    // 支持移动语义 (用于智能指针管理)
    GBuffer(GBuffer&&) = default;
    GBuffer& operator=(GBuffer&&) = default;
};

} // namespace enigma::graphic