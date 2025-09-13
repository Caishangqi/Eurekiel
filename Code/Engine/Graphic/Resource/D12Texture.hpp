#pragma once
#include "D12Resources.hpp"

namespace enigma::graphic
{
    /**
 * @brief D12Texture类 - 专用于延迟渲染的纹理封装
 * 
 * 教学要点:
 * 1. 专门为G-Buffer、Shadow Map、后处理RT设计
 * 2. 支持多种纹理格式和用途 (Render Target, Shader Resource, UAV)
 * 3. 集成描述符创建，简化Bindless绑定
 * 4. 支持Mipmap生成和多重采样
 * 
 * 延迟渲染特性:
 * - 支持MRT (Multiple Render Targets)
 * - 优化的G-Buffer格式支持
 * - 高效的RT切换和状态管理
 */
    class D12Texture : public D12Resource
    {
    private:
        uint32_t             m_width; // 纹理宽度
        uint32_t             m_height; // 纹理高度
        uint32_t             m_mipLevels; // Mip层级数
        DXGI_FORMAT          m_format; // 像素格式
        D3D12_RESOURCE_FLAGS m_flags; // 资源标志

        // 描述符句柄 (用于Bindless绑定)
        D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle; // Shader Resource View
        D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandle; // Render Target View  
        D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle; // Depth Stencil View
        D3D12_CPU_DESCRIPTOR_HANDLE m_uavHandle; // Unordered Access View

        bool m_hasSRV; // 是否创建了SRV
        bool m_hasRTV; // 是否创建了RTV
        bool m_hasDSV; // 是否创建了DSV
        bool m_hasUAV; // 是否创建了UAV

    public:
        /**
         * @brief 纹理用途枚举
         * 
         * 教学要点: 明确纹理的用途，用于优化创建参数
         */
        enum class Usage
        {
            RenderTarget, // 渲染目标 (G-Buffer RT)
            DepthStencil, // 深度模板缓冲
            ShaderResource, // 着色器资源 (采样纹理)
            UnorderedAccess, // 无序访问 (Compute Shader读写)
            RenderTargetAndShaderResource, // 既是RT又是SRV (常用于后处理)
            DepthStencilAndShaderResource // 既是DSV又是SRV (Shadow Map)
        };

        /**
         * @brief 构造函数
         */
        D12Texture();

        /**
         * @brief 析构函数
         */
        virtual ~D12Texture();

        /**
         * @brief 创建2D纹理
         * @param device DX12设备
         * @param width 纹理宽度
         * @param height 纹理高度
         * @param format 像素格式
         * @param usage 纹理用途
         * @param mipLevels Mip层级数 (0表示自动生成完整Mip链)
         * @param sampleCount 多重采样数量
         * @param clearValue 清除值 (用于RT和DSV优化)
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: 
         * 1. 根据用途自动设置资源标志和初始状态
         * 2. 创建对应的描述符视图
         * 3. 支持多重采样抗锯齿 (MSAA)
         */
        bool Create2D(ID3D12Device*            device,
                      uint32_t                 width, uint32_t height,
                      DXGI_FORMAT              format, Usage   usage,
                      uint32_t                 mipLevels   = 1,
                      uint32_t                 sampleCount = 1,
                      const D3D12_CLEAR_VALUE* clearValue  = nullptr);

        /**
         * @brief 创建G-Buffer专用RT
         * @param device DX12设备
         * @param width RT宽度
         * @param height RT高度
         * @param format RT格式 (RGBA8, RGBA16F等)
         * @param rtIndex G-Buffer索引 (0-3为主G-Buffer, 4-9为临时缓冲)
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: G-Buffer RT的特殊优化，包括清除值和格式选择
         */
        bool CreateAsGBufferRT(ID3D12Device* device,
                               uint32_t      width, uint32_t  height,
                               DXGI_FORMAT   format, uint32_t rtIndex);

        /**
         * @brief 创建Shadow Map
         * @param device DX12设备
         * @param size Shadow Map尺寸 (正方形)
         * @param cascadeCount 级联阴影层数
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: Shadow Map的特殊需求，深度格式和比较采样
         */
        bool CreateAsShadowMap(ID3D12Device* device, uint32_t size, uint32_t cascadeCount = 1);

        // ========================================================================
        // 属性访问接口
        // ========================================================================

        uint32_t    GetWidth() const { return m_width; }
        uint32_t    GetHeight() const { return m_height; }
        uint32_t    GetMipLevels() const { return m_mipLevels; }
        DXGI_FORMAT GetFormat() const { return m_format; }

        // 描述符句柄访问
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const { return m_srvHandle; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return m_rtvHandle; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return m_dsvHandle; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetUAVHandle() const { return m_uavHandle; }

        // 描述符可用性检查
        bool HasSRV() const { return m_hasSRV; }
        bool HasRTV() const { return m_hasRTV; }
        bool HasDSV() const { return m_hasDSV; }
        bool HasUAV() const { return m_hasUAV; }

    private:
        /**
         * @brief 创建描述符视图
         * @param device DX12设备
         * @param usage 纹理用途
         */
        void CreateDescriptorViews(ID3D12Device* device, Usage usage);

        /**
         * @brief 根据用途确定资源标志
         * @param usage 纹理用途
         * @return 资源标志
         */
        static D3D12_RESOURCE_FLAGS GetResourceFlags(Usage usage);

        /**
         * @brief 根据用途确定初始资源状态
         * @param usage 纹理用途
         * @return 初始资源状态
         */
        static D3D12_RESOURCE_STATES GetInitialState(Usage usage);
    };
}
