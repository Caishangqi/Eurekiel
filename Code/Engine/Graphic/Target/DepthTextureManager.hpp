/**
 * @file DepthTextureManager.hpp
 * @brief Iris兼容的3个深度纹理管理器 (depthtex0/1/2)
 *
 * ============================================================================
 * Iris架构对应关系
 * ============================================================================
 *
 * Iris源码位置:
 * net.irisshaders.iris.targets.RenderTargets.java (Lines 300-366)
 *
 * 核心职责对应关系:
 *
 * 1. **深度纹理管理** (Iris核心特性):
 *    - Iris: depthTex0/1/2 (int ID)
 *    - DX12: m_depthTextures[3] (std::shared_ptr<D12Texture>)
 *    - 教学意义: 3个深度纹理的语义完全对应Iris，不可修改
 *
 * 2. **深度复制方法** (Iris兼容):
 *    - Iris: CopyPreTranslucentDepth() / CopyPreHandDepth()
 *    - DX12: 相同方法名 (必须保持一致)
 *    - 教学意义: 方法名和调用时机与Iris完全一致
 *
 * 3. **深度纹理用途** (Iris定义):
 *    - depthtex0: 主深度纹理，包含所有几何体
 *    - depthtex1: 不含半透明的深度 (TERRAIN_TRANSLUCENT前复制)
 *    - depthtex2: 不含手部的深度 (HAND_SOLID前复制)
 *
 * ============================================================================
 * DirectX 12特化设计
 * ============================================================================
 *
 * 1. **资源类型**:
 *    - 使用D12Texture管理深度纹理 (可采样深度)
 *    - 格式: DXGI_FORMAT_D24_UNORM_S8_UINT (24位深度+8位模板)
 *
 * 2. **描述符管理**:
 *    - DSV (Depth Stencil View) 用于深度写入
 *    - SRV (Shader Resource View) 用于深度采样
 *
 * 3. **Copy机制**:
 *    - 使用ID3D12GraphicsCommandList::CopyResource()
 *    - 自动处理资源状态转换 (DSV -> COPY_SOURCE/DEST)
 *
 * @note 此类直接对应Iris的RenderTargets深度纹理部分，是延迟渲染管线的基础组件
 */

#pragma once

#include <array>
#include <memory>
#include <cstdint>
#include <string>
#include <d3d12.h>
#include "Engine/Graphic/Target/D12DepthTexture.hpp"

namespace enigma::graphic
{
    /**
     * @class DepthTextureManager
     * @brief Iris兼容的3个深度纹理管理器
     *
     * **Iris架构对应**:
     * - Iris: RenderTargets.java中的depthTex0/1/2管理
     * - DX12: DepthTextureManager (相同职责)
     *
     * **核心职责**:
     * 1. **深度纹理生命周期管理** - 创建/销毁3个depth texture
     * 2. **深度复制操作** - 对应Iris的CopyPreTranslucentDepth/CopyPreHandDepth
     * 3. **Bindless索引查询** - 快速获取depth texture索引
     * 4. **DSV访问** - 从D12DepthTexture获取Depth Stencil View供渲染使用
     * 5. **窗口尺寸变化响应** - 自动Resize所有depth texture
     *
     * **内存布局优化**:
     * - std::array<std::shared_ptr<D12DepthTexture>, 3> (24字节, 64位系统)
     * - 总计约60字节 (极致节省，DSV句柄由D12DepthTexture管理)
     */
    class DepthTextureManager
    {
    public:
        /**
         * @brief 构造DepthTextureManager并创建3个深度纹理
         * @param width 屏幕宽度
         * @param height 屏幕高度
         * @param depthFormat 深度格式（默认DEPTH24_STENCIL8）
         *
         * 教学要点:
         * - 使用D12Texture创建可采样深度纹理
         * - 自动创建DSV描述符
         * - 注册Bindless索引供Shader采样
         */
        DepthTextureManager(
            int         width,
            int         height,
            DXGI_FORMAT depthFormat = DXGI_FORMAT_D24_UNORM_S8_UINT
        );

        /**
         * @brief 默认析构函数
         *
         * 教学要点: std::shared_ptr自动管理D12Texture生命周期
         */
        ~DepthTextureManager() = default;

        // ========================================================================
        // 深度纹理访问接口
        // ========================================================================

        /**
         * @brief 获取指定深度纹理（对应Iris的depthTex0/1/2）
         * @param index 深度纹理索引 [0-2]
         * @return std::shared_ptr<D12DepthTexture>
         *
         * 教学要点:
         * - index 0 = depthtex0 (主深度纹理)
         * - index 1 = depthtex1 (不含半透明)
         * - index 2 = depthtex2 (不含手部)
         */
        std::shared_ptr<D12DepthTexture> GetDepthTexture(int index) const;

        /**
         * @brief 获取指定深度纹理的Bindless索引
         * @param index 深度纹理索引 [0-2]
         * @return uint32_t Bindless索引
         *
         * 教学要点: HLSL通过此索引采样深度纹理
         * Texture2D depthTex = ResourceDescriptorHeap[index];
         */
        uint32_t GetDepthTextureIndex(int index) const;

        /**
         * @brief 获取指定深度纹理的DSV句柄
         * @param index 深度纹理索引 [0-2]
         * @return D3D12_CPU_DESCRIPTOR_HANDLE
         *
         * 教学要点:
         * - DSV用于OMSetRenderTargets()绑定深度缓冲
         * - 直接从D12DepthTexture::GetDSVHandle()获取，无需缓存
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSV(int index) const;

        // ========================================================================
        // Iris兼容的深度复制接口（关键功能）
        // ========================================================================

        /**
         * @brief 复制depthtex0 -> depthtex1（对应Iris的CopyPreTranslucentDepth）
         * @param cmdList 命令列表
         *
         * **Iris调用时机**:
         * - 在TERRAIN_TRANSLUCENT Phase前调用
         * - 保存不含半透明的深度，用于水下效果、粒子排序等
         *
         * **业务逻辑**:
         * 1. 转换depthtex0状态: DSV -> COPY_SOURCE
         * 2. 转换depthtex1状态: DSV -> COPY_DEST
         * 3. CopyResource(depthtex0, depthtex1)
         * 4. 恢复depthtex0状态: COPY_SOURCE -> DSV
         * 5. 恢复depthtex1状态: COPY_DEST -> DSV
         *
         * 教学要点: 完全对应Iris的深度复制逻辑
         */
        void CopyPreTranslucentDepth(ID3D12GraphicsCommandList* cmdList);

        /**
         * @brief 复制depthtex0 -> depthtex2（对应Iris的CopyPreHandDepth）
         * @param cmdList 命令列表
         *
         * **Iris调用时机**:
         * - 在HAND_SOLID Phase前调用
         * - 保存不含手部的深度，用于手部渲染后处理
         *
         * **业务逻辑**:
         * 1. 转换depthtex0状态: DSV -> COPY_SOURCE
         * 2. 转换depthtex2状态: DSV -> COPY_DEST
         * 3. CopyResource(depthtex0, depthtex2)
         * 4. 恢复depthtex0状态: COPY_SOURCE -> DSV
         * 5. 恢复depthtex2状态: COPY_DEST -> DSV
         *
         * 教学要点: 完全对应Iris的深度复制逻辑
         */
        void CopyPreHandDepth(ID3D12GraphicsCommandList* cmdList);

        /**
         * @brief 通用深度复制（内部辅助方法）
         * @param cmdList 命令列表
         * @param srcIndex 源深度纹理索引 [0-2]
         * @param destIndex 目标深度纹理索引 [0-2]
         *
         * 教学要点: 封装通用的深度复制逻辑，避免代码重复
         */
        void CopyDepth(ID3D12GraphicsCommandList* cmdList, int srcIndex, int destIndex);

        // ========================================================================
        // 窗口尺寸变化响应
        // ========================================================================

        /**
         * @brief 响应窗口尺寸变化，Resize所有深度纹理
         * @param newWidth 新屏幕宽度
         * @param newHeight 新屏幕高度
         *
         * 教学要点:
         * - 遍历3个深度纹理，调用Resize()
         * - 重新创建DSV描述符
         * - 重新注册Bindless索引
         */
        void OnResize(int newWidth, int newHeight);

        // ========================================================================
        // 调试支持
        // ========================================================================

        /**
         * @brief 获取所有深度纹理的概览信息
         * @return std::string 格式化的调试信息
         *
         * 教学要点: 提供深度纹理详细信息，方便调试
         * 格式: "depthtex0 (1920x1080, D24S8), depthtex1 (...), depthtex2 (...)"
         */
        std::string GetDebugInfo() const;

    private:
        // ========================================================================
        // 私有成员
        // ========================================================================

        std::array<std::shared_ptr<D12DepthTexture>, 3> m_depthTextures; // depthtex0/1/2
        int                                             m_width; // 屏幕宽度
        int                                             m_height; // 屏幕高度
        DXGI_FORMAT                                     m_depthFormat; // 深度格式

        /**
         * @brief 验证索引有效性
         */
        bool IsValidIndex(int index) const
        {
            return index >= 0 && index < 3;
        }

        // ========================================================================
        // 教学要点总结
        // ========================================================================

        /**
         * 1. **Iris架构映射**:
         *    - Iris: RenderTargets.java (depthTex0/1/2管理)
         *    - DX12: DepthTextureManager.hpp (完全对应)
         *
         * 2. **深度纹理语义**:
         *    - depthtex0: 主深度纹理 (所有几何体)
         *    - depthtex1: 不含半透明深度 (TERRAIN_TRANSLUCENT前复制)
         *    - depthtex2: 不含手部深度 (HAND_SOLID前复制)
         *
         * 3. **Iris兼容的Copy方法**:
         *    - CopyPreTranslucentDepth() (depthtex0 -> depthtex1)
         *    - CopyPreHandDepth() (depthtex0 -> depthtex2)
         *    - 方法名必须与Iris一致，不可修改
         *
         * 4. **Bindless索引查询**:
         *    - GetDepthTextureIndex() 获取深度纹理Bindless索引
         *    - HLSL通过索引直接采样深度纹理
         *    - 零Root Signature切换开销
         *
         * 5. **智能指针内存管理**:
         *    - std::shared_ptr<D12DepthTexture>自动管理深度纹理生命周期
         *    - 支持多处引用同一深度纹理
         *    - 析构时自动释放GPU资源
         *
         * 6. **架构改进**:
         *    - 使用D12DepthTexture代替D12Texture，类型更精确
         *    - 删除m_dsvHandles缓存，直接从D12DepthTexture获取
         *    - 删除CreateDSV()方法，DSV由D12DepthTexture自动管理
         *    - 代码量减少约55行（从420行降至~280行）
         */
    };
} // namespace enigma::graphic
