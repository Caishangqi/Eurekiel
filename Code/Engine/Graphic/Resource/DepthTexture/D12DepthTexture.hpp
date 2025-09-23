/**
 * @file D12DepthTexture.hpp
 * @brief DirectX 12深度纹理封装 - 独立的深度纹理资源管理
 *
 * 教学重点:
 * 1. 深度纹理的特殊性和管理方式
 * 2. DirectX 12深度缓冲区的创建和配置
 * 3. 深度纹理的格式选择和性能考虑
 * 4. 与普通纹理的区别和专用功能
 *
 * 架构设计:
 * - 继承D12Resource，提供统一的资源管理
 * - 专门用于深度测试和阴影贴图
 * - 支持深度读取和深度写入模式
 * - 对应Iris中深度纹理的概念
 */

#pragma once

#include "../D12Resources.hpp"
#include "../Texture/D12Texture.hpp"
#include <memory>
#include <string>
#include <dxgi1_6.h>

namespace enigma::graphic
{
    /**
     * @brief DirectX 12深度纹理封装类
     * @details 专门用于深度测试、阴影贴图等深度相关渲染
     *
     * 核心特性:
     * - 专用深度格式支持 (D24_UNORM_S8_UINT、D32_FLOAT等)
     * - 深度模板视图(DSV)自动管理
     * - 支持深度纹理采样 (SRV)
     * - 阴影贴图支持
     *
     * 与普通纹理的区别:
     * - 只支持深度格式
     * - 自动创建DSV
     * - 可选择是否支持纹理采样
     * - 专门的深度清除操作
     */
    class D12DepthTexture : public D12Resource
    {
    public:
        /**
         * @brief 深度纹理类型枚举
         */
        enum class DepthType
        {
            DepthOnly, ///< 仅深度 (D32_FLOAT)
            DepthStencil, ///< 深度+模板 (D24_UNORM_S8_UINT)
            ShadowMap ///< 阴影贴图 (D32_FLOAT, 支持采样)
        };

    private:
        // ==================== 核心资源 ====================
        std::unique_ptr<D12Texture> m_depthTexture; ///< 底层深度纹理

        // ==================== 视图管理 ====================
        D3D12_CPU_DESCRIPTOR_HANDLE m_dsvHandle; ///< 深度模板视图句柄
        D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle; ///< 着色器资源视图句柄 (可选)
        bool                        m_hasSRV; ///< 是否支持着色器采样

        // ==================== 属性管理 ====================
        std::string m_name; ///< 深度纹理名称
        uint32_t    m_width; ///< 宽度
        uint32_t    m_height; ///< 高度
        DXGI_FORMAT m_depthFormat; ///< 深度格式
        DepthType   m_depthType; ///< 深度纹理类型

        // ==================== 状态管理 ====================
        float   m_clearDepth; ///< 默认清除深度值
        uint8_t m_clearStencil; ///< 默认清除模板值
        bool    m_isValid; ///< 是否有效

    public:
        /**
         * @brief 构造函数
         * @param name 深度纹理名称
         * @param width 宽度
         * @param height 高度
         * @param depthType 深度纹理类型
         * @param clearDepth 默认清除深度值
         * @param clearStencil 默认清除模板值
         */
        D12DepthTexture(
            const std::string& name,
            uint32_t           width,
            uint32_t           height,
            DepthType          depthType    = DepthType::DepthStencil,
            float              clearDepth   = 1.0f,
            uint8_t            clearStencil = 0
        );

        /**
         * @brief 析构函数
         * @details 自动清理深度纹理资源和视图
         */
        virtual ~D12DepthTexture();

        // ==================== 纹理访问接口 ====================

        /**
         * @brief 获取底层深度纹理
         * @return 深度纹理指针
         */
        D12Texture* GetDepthTexture() const { return m_depthTexture.get(); }

        /**
         * @brief 获取深度模板视图句柄
         * @return DSV句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return m_dsvHandle; }

        /**
         * @brief 获取着色器资源视图句柄
         * @return SRV句柄 (如果支持采样)
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const;

        /**
         * @brief 检查是否支持着色器采样
         * @return 是否可以作为纹理采样
         */
        bool HasShaderResourceView() const { return m_hasSRV; }

        // ==================== 尺寸和属性管理 ====================

        /**
         * @brief 获取宽度
         * @return 宽度像素数
         */
        uint32_t GetWidth() const { return m_width; }

        /**
         * @brief 获取高度
         * @return 高度像素数
         */
        uint32_t GetHeight() const { return m_height; }

        /**
         * @brief 获取深度格式
         * @return DXGI深度格式
         */
        DXGI_FORMAT GetDepthFormat() const { return m_depthFormat; }

        /**
         * @brief 获取深度纹理类型
         * @return 深度类型枚举
         */
        DepthType GetDepthType() const { return m_depthType; }

        /**
         * @brief 获取名称
         * @return 深度纹理名称
         */
        const std::string& GetName() const { return m_name; }

        /**
         * @brief 检查是否有效
         * @return 是否处于有效状态
         */
        bool IsValid() const { return m_isValid; }

        // ==================== 深度操作接口 ====================

        /**
         * @brief 清除深度缓冲区
         * @param cmdList 命令列表
         * @param depthValue 深度清除值 (默认使用构造时设置的值)
         * @param stencilValue 模板清除值 (默认使用构造时设置的值)
         */
        void Clear(
            ID3D12GraphicsCommandList* cmdList,
            float                      depthValue   = -1.0f,
            uint8_t                    stencilValue = 255
        );

        /**
         * @brief 绑定为深度目标
         * @param cmdList 命令列表
         * @details 设置当前深度纹理为渲染目标的深度缓冲区
         */
        void BindAsDepthTarget(ID3D12GraphicsCommandList* cmdList);

        // ==================== 动态调整接口 ====================

        /**
         * @brief 调整深度纹理尺寸
         * @param newWidth 新宽度
         * @param newHeight 新高度
         * @return 是否成功调整
         */
        bool Resize(uint32_t newWidth, uint32_t newHeight);

        // ==================== 调试支持 ====================

        /**
         * @brief 获取调试信息
         * @return 包含尺寸、格式、类型的调试字符串
         */
        std::string GetDebugInfo() const;

        // ==================== 静态辅助方法 ====================

        /**
         * @brief 将深度类型转换为DXGI格式
         * @param depthType 深度类型
         * @return 对应的DXGI格式
         */
        static DXGI_FORMAT GetFormatFromDepthType(DepthType depthType);

        /**
         * @brief 获取对应的类型化格式 (用于SRV)
         * @param depthFormat 深度格式
         * @return 类型化格式 (如果支持)
         */
        static DXGI_FORMAT GetTypedFormat(DXGI_FORMAT depthFormat);

    private:
        // ==================== 内部辅助方法 ====================

        /**
         * @brief 创建深度纹理资源
         * @return 是否创建成功
         */
        bool CreateDepthTexture();

        /**
         * @brief 创建深度模板视图
         * @return 是否创建成功
         */
        bool CreateDepthStencilView();

        /**
         * @brief 创建着色器资源视图 (如果支持)
         * @return 是否创建成功
         */
        bool CreateShaderResourceView();

        /**
         * @brief 设置调试名称
         */
        void SetDebugName();
    };

    // ==================== 类型别名 ====================
    using DepthTexturePtr = std::unique_ptr<D12DepthTexture>;
} // namespace enigma::graphic
