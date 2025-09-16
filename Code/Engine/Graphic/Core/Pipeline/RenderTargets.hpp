/**
 * @file RenderTargets.hpp
 * @brief Enigma渲染目标管理器 - 基于Iris RenderTargets.java的DirectX 12实现
 * 
 * 教学重点:
 * 1. 理解Iris真实的渲染目标管理架构模式
 * 2. 学习G-Buffer管理的现代化实现方式
 * 3. 掌握多渲染目标(MRT)的动态创建和管理
 * 4. 理解深度纹理管理和阶段性拷贝策略
 * 
 * 架构对应关系:
 * - 本类 ←→ net.irisshaders.iris.targets.RenderTargets.java
 * - 管理colortex0到colortexN的多个渲染目标
 * - 支持深度纹理管理（depthtex1, depthtex2）
 * - 动态创建和调整渲染目标大小
 * - 帧缓冲区创建和管理
 */

#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>

#include "../../Resource/Texture/D12Texture.hpp"
#include "../DX12/D3D12RenderSystem.hpp"
#include "WorldRenderingPhase.hpp"

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace enigma::graphic
{
    // 前向声明
    class D12Texture;
    struct PackRenderTargetDirectives;
    struct PackDirectives;

    /**
     * @brief 渲染目标设置配置
     * @details 对应Iris中的PackRenderTargetDirectives.RenderTargetSettings
     */
    struct RenderTargetSettings
    {
        DXGI_FORMAT internalFormat = DXGI_FORMAT_R8G8B8A8_UNORM; ///< 内部像素格式
        uint32_t    textureScale   = 1; ///< 纹理缩放因子
        bool        mipmaps        = false; ///< 是否生成Mipmap
        bool        clear          = true; ///< 是否需要清除
        float       clearColor[4]  = {0.0f, 0.0f, 0.0f, 0.0f}; ///< 清除颜色
        std::string debugName; ///< 调试名称

        /**
         * @brief 默认构造函数
         */
        RenderTargetSettings() = default;

        /**
         * @brief 构造函数
         * @param format 像素格式
         * @param scale 缩放因子
         * @param name 调试名称
         */
        RenderTargetSettings(DXGI_FORMAT format, uint32_t scale = 1, const std::string& name = "")
            : internalFormat(format), textureScale(scale), debugName(name)
        {
        }
    };

    /**
     * @brief 深度纹理包装器
     * @details 对应Iris中的DepthTexture类
     */
    class DepthTexture
    {
    private:
        std::unique_ptr<D12Texture> m_texture; ///< 底层纹理对象
        std::string                 m_name; ///< 纹理名称
        uint32_t                    m_width; ///< 宽度
        uint32_t                    m_height; ///< 高度
        DXGI_FORMAT                 m_format; ///< 深度格式

    public:
        /**
         * @brief 构造函数
         * @param name 纹理名称
         * @param width 宽度
         * @param height 高度
         * @param format 深度格式
         */
        DepthTexture(const std::string& name, uint32_t width, uint32_t height, DXGI_FORMAT format);

        /**
         * @brief 析构函数
         */
        ~DepthTexture();

        /**
         * @brief 调整大小
         * @param width 新宽度
         * @param height 新高度
         * @param format 新格式
         */
        void Resize(uint32_t width, uint32_t height, DXGI_FORMAT format);

        /**
         * @brief 销毁资源
         */
        void Destroy();

        /**
         * @brief 获取纹理ID
         * @return 纹理资源指针
         */
        ID3D12Resource* GetTextureResource() const;

        /**
         * @brief 获取深度模板视图
         * @return DSV句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const;

        /**
         * @brief 获取着色器资源视图
         * @return SRV句柄
         */
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const;

        /**
         * @brief 获取纹理名称
         */
        const std::string& GetName() const { return m_name; }

        /**
         * @brief 获取当前尺寸
         */
        uint32_t    GetWidth() const { return m_width; }
        uint32_t    GetHeight() const { return m_height; }
        DXGI_FORMAT GetFormat() const { return m_format; }
    };

    /**
     * @brief 渲染目标管理器 - 对应Iris RenderTargets.java
     * @details
     * 这个类是延迟渲染系统的核心组件，负责：
     * - 管理colortex0到colortexN的多个颜色渲染目标
     * - 管理depthtex1和depthtex2深度纹理
     * - 动态创建和销毁渲染目标
     * - 处理渲染目标的大小调整
     * - 创建和管理帧缓冲区对象
     * 
     * 教学要点:
     * - 理解为什么需要多个渲染目标而不是单个GBuffer
     * - 学习延迟渲染中的数据流管理
     * - 掌握GPU资源的生命周期管理
     * - 理解深度纹理在不同渲染阶段的作用
     * 
     * Iris对应关系:
     * - targets[] ←→ RenderTarget[] targets
     * - noTranslucents ←→ DepthTexture noTranslucents
     * - noHand ←→ DepthTexture noHand
     * - targetSettingsMap ←→ Map<Integer, RenderTargetSettings>
     * - resizeIfNeeded() ←→ resizeIfNeeded(...)
     * - createFramebufferWritingToMain() ←→ createFramebufferWritingToMain(int[])
     * 
     * @note 这是基于真实Iris源码的实现，确保架构的准确性
     */
    class RenderTargets
    {
    public:
        /**
         * @brief 渲染目标统计信息
         * @details 用于调试和性能分析的统计数据
         */
        struct Statistics
        {
            uint32_t activeTargets       = 0; ///< 活跃的渲染目标数量
            uint32_t totalTargetsCreated = 0; ///< 总共创建的渲染目标数量
            uint32_t framebuffersCreated = 0; ///< 创建的帧缓冲区数量
            uint32_t resizeOperations    = 0; ///< 大小调整操作次数
            size_t   totalMemoryUsed     = 0; ///< 总内存使用量（字节）
            bool     fullClearRequired   = false; ///< 是否需要完全清除

            /**
             * @brief 重置统计信息
             */
            void Reset()
            {
                activeTargets       = 0;
                framebuffersCreated = 0;
                resizeOperations    = 0;
            }
        };

    public:
        /**
         * @brief 构造函数
         * @param width 初始宽度
         * @param height 初始高度
         * @param depthResource 主深度纹理资源
         * @param depthFormat 深度缓冲格式
         * @param renderTargets 渲染目标设置映射
         * @details 
         * 对应Iris构造函数：
         * RenderTargets(int width, int height, int depthTexture, int depthBufferVersion, 
         *               DepthBufferFormat depthFormat, Map<Integer, RenderTargetSettings>, 
         *               PackDirectives packDirectives)
         */
        RenderTargets(uint32_t                                                  width, uint32_t height,
                      ID3D12Resource*                                           depthResource,
                      DXGI_FORMAT                                               depthFormat,
                      const std::unordered_map<uint32_t, RenderTargetSettings>& renderTargets);

        /**
         * @brief 析构函数
         * @details 确保所有渲染目标资源得到正确释放
         */
        ~RenderTargets();

        // 禁用拷贝构造和赋值 - 渲染目标管理器应该是唯一的
        RenderTargets(const RenderTargets&)            = delete;
        RenderTargets& operator=(const RenderTargets&) = delete;

        // 启用移动构造和赋值 - 支持高效的资源转移
        RenderTargets(RenderTargets&&) noexcept            = default;
        RenderTargets& operator=(RenderTargets&&) noexcept = default;

        // ==================== 核心渲染目标管理接口 ====================

        /**
         * @brief 销毁所有渲染目标
         * @details 
         * 对应Iris方法：public void destroy()
         * 释放所有渲染目标、深度纹理和帧缓冲区资源
         */
        void Destroy();

        /**
         * @brief 获取渲染目标数量
         * @return 渲染目标总数
         * @details 对应Iris方法：public int getRenderTargetCount()
         */
        uint32_t GetRenderTargetCount() const { return static_cast<uint32_t>(m_targets.size()); }

        /**
         * @brief 获取指定索引的渲染目标
         * @param index 渲染目标索引
         * @return 渲染目标指针，如果不存在返回nullptr
         * @details 
         * 对应Iris方法：public RenderTarget get(int index)
         * 返回已存在的渲染目标，不会创建新的
         */
        D12Texture* Get(uint32_t index) const;

        /**
         * @brief 获取或创建指定索引的渲染目标
         * @param index 渲染目标索引
         * @return 渲染目标指针
         * @details 
         * 对应Iris方法：public RenderTarget getOrCreate(int index)
         * 如果渲染目标不存在，会根据设置自动创建
         */
        D12Texture* GetOrCreate(uint32_t index);

        /**
         * @brief 获取主深度纹理资源
         * @return 深度纹理资源指针
         * @details 对应Iris方法：public int getDepthTexture()
         */
        ID3D12Resource* GetDepthTexture() const { return m_currentDepthTexture; }

        /**
         * @brief 获取无半透明物体的深度纹理
         * @return 深度纹理对象
         * @details 对应Iris方法：public DepthTexture getDepthTextureNoTranslucents()
         */
        DepthTexture* GetDepthTextureNoTranslucents() const { return m_noTranslucents.get(); }

        /**
         * @brief 获取无手部渲染的深度纹理
         * @return 深度纹理对象
         * @details 对应Iris方法：public DepthTexture getDepthTextureNoHand()
         */
        DepthTexture* GetDepthTextureNoHand() const { return m_noHand.get(); }

        // ==================== 动态管理接口 ====================

        /**
         * @brief 根据需要调整渲染目标大小
         * @param newDepthResource 新的深度纹理资源
         * @param newWidth 新宽度
         * @param newHeight 新高度
         * @param newDepthFormat 新深度格式
         * @return true表示发生了大小变化
         * @details 
         * 对应Iris方法：
         * public boolean resizeIfNeeded(int newDepthBufferVersion, int newDepthTextureId, 
         *                               int newWidth, int newHeight, DepthBufferFormat newDepthFormat, 
         *                               PackDirectives packDirectives)
         * 
         * 核心逻辑:
         * 1. 检查深度纹理、尺寸、格式是否发生变化
         * 2. 重新分配深度缓冲区和颜色渲染目标
         * 3. 更新帧缓冲区绑定
         * 4. 标记需要完全清除
         */
        bool ResizeIfNeeded(ID3D12Resource* newDepthResource,
                            uint32_t        newWidth, uint32_t newHeight,
                            DXGI_FORMAT     newDepthFormat);

        /**
         * @brief 拷贝半透明渲染前的深度信息
         * @details 
         * 对应Iris方法：public void copyPreTranslucentDepth()
         * 用于保存不透明物体渲染完成后的深度信息
         */
        void CopyPreTranslucentDepth();

        /**
         * @brief 拷贝手部渲染前的深度信息
         * @details 
         * 对应Iris方法：public void copyPreHandDepth()
         * 用于保存主世界渲染完成后的深度信息
         */
        void CopyPreHandDepth();

        // ==================== 帧缓冲区创建接口 ====================

        /**
         * @brief 创建写入主纹理的帧缓冲区
         * @param drawBuffers 绘制缓冲区索引数组
         * @param bufferCount 缓冲区数量
         * @return 创建的帧缓冲区对象
         * @details 
         * 对应Iris方法：public GlFramebuffer createFramebufferWritingToMain(int[] drawBuffers)
         * 
         * 教学要点：在DirectX 12中，帧缓冲区概念由渲染目标视图(RTV)和深度模板视图(DSV)组合实现
         */
        Microsoft::WRL::ComPtr<ID3D12Resource> CreateFramebufferWritingToMain(const uint32_t* drawBuffers, uint32_t bufferCount);

        /**
         * @brief 创建写入备用纹理的帧缓冲区
         * @param drawBuffers 绘制缓冲区索引数组
         * @param bufferCount 缓冲区数量
         * @return 创建的帧缓冲区对象
         * @details 对应Iris方法：public GlFramebuffer createFramebufferWritingToAlt(int[] drawBuffers)
         */
        Microsoft::WRL::ComPtr<ID3D12Resource> CreateFramebufferWritingToAlt(const uint32_t* drawBuffers, uint32_t bufferCount);

        /**
         * @brief 创建G-Buffer帧缓冲区
         * @param stageWritesToAlt 写入备用纹理的阶段集合
         * @param drawBuffers 绘制缓冲区数组
         * @param bufferCount 缓冲区数量
         * @return 创建的帧缓冲区对象
         * @details 
         * 对应Iris方法：public GlFramebuffer createGbufferFramebuffer(ImmutableSet<Integer> stageWritesToAlt, int[] drawBuffers)
         * 
         * 用于G-Buffer阶段的多渲染目标设置
         */
        Microsoft::WRL::ComPtr<ID3D12Resource> CreateGbufferFramebuffer(
            const std::vector<uint32_t>& stageWritesToAlt,
            const uint32_t*              drawBuffers, uint32_t bufferCount);

        // ==================== 状态查询和管理接口 ====================

        /**
         * @brief 检查是否需要完全清除
         * @return true表示需要完全清除
         * @details 对应Iris方法：public boolean isFullClearRequired()
         */
        bool IsFullClearRequired() const { return m_fullClearRequired; }

        /**
         * @brief 标记完全清除已完成
         * @details 对应Iris方法：public void onFullClear()
         */
        void OnFullClear() { m_fullClearRequired = false; }

        /**
         * @brief 获取当前宽度
         * @return 当前渲染目标宽度
         * @details 对应Iris方法：public int getCurrentWidth()
         */
        uint32_t GetCurrentWidth() const { return m_cachedWidth; }

        /**
         * @brief 获取当前高度
         * @return 当前渲染目标高度
         * @details 对应Iris方法：public int getCurrentHeight()
         */
        uint32_t GetCurrentHeight() const { return m_cachedHeight; }

        /**
         * @brief 检查是否已销毁
         * @return true表示已销毁
         */
        bool IsDestroyed() const { return m_destroyed; }

        // ==================== 调试和统计接口 ====================

        /**
         * @brief 获取渲染目标统计信息
         * @return 统计信息结构体
         * @details 用于性能分析和调试
         */
        const Statistics& GetStatistics() const { return m_statistics; }

        /**
         * @brief 获取渲染目标调试名称
         * @param index 渲染目标索引
         * @return 调试名称字符串
         */
        std::string GetRenderTargetDebugName(uint32_t index) const;

        /**
         * @brief 验证渲染目标设置
         * @return true表示所有设置有效
         */
        bool ValidateSettings() const;

    private:
        // ==================== 内部辅助方法 ====================

        /**
         * @brief 创建指定索引的渲染目标
         * @param index 渲染目标索引
         * @details 
         * 对应Iris方法：private void create(int index)
         * 根据设置创建新的渲染目标
         */
        void Create(uint32_t index);

        /**
         * @brief 初始化深度纹理
         * @param width 宽度
         * @param height 高度
         * @param format 深度格式
         */
        void InitializeDepthTextures(uint32_t width, uint32_t height, DXGI_FORMAT format);

        /**
         * @brief 更新统计信息
         */
        void UpdateStatistics();

        /**
         * @brief 验证索引有效性
         * @param index 要验证的索引
         * @return true表示索引有效
         */
        bool ValidateIndex(uint32_t index) const;

        /**
         * @brief 计算内存使用量
         * @return 总内存使用量（字节）
         */
        size_t CalculateMemoryUsage() const;

    private:
        // ==================== 核心成员变量 ====================

        /// 渲染目标数组 - 对应Iris的targets[]
        std::vector<std::unique_ptr<D12Texture>> m_targets;

        /// 无半透明物体深度纹理 - 对应Iris的noTranslucents
        std::unique_ptr<DepthTexture> m_noTranslucents;

        /// 无手部渲染深度纹理 - 对应Iris的noHand
        std::unique_ptr<DepthTexture> m_noHand;

        /// 渲染目标设置映射 - 对应Iris的targetSettingsMap
        std::unordered_map<uint32_t, RenderTargetSettings> m_targetSettingsMap;

        /// 当前深度纹理资源 - 对应Iris的currentDepthTexture
        ID3D12Resource* m_currentDepthTexture;

        /// 当前深度格式 - 对应Iris的currentDepthFormat
        DXGI_FORMAT m_currentDepthFormat;

        /// 缓存的尺寸信息
        uint32_t m_cachedWidth;
        uint32_t m_cachedHeight;

        /// 状态标志
        bool m_fullClearRequired; ///< 是否需要完全清除 - 对应Iris的fullClearRequired
        bool m_translucentDepthDirty; ///< 半透明深度是否脏 - 对应Iris的translucentDepthDirty
        bool m_handDepthDirty; ///< 手部深度是否脏 - 对应Iris的handDepthDirty
        bool m_destroyed; ///< 是否已销毁 - 对应Iris的destroyed

        /// 统计信息
        mutable Statistics m_statistics;

        /// 初始化状态标志
        bool m_isInitialized = false;
    };
} // namespace enigma::graphic
