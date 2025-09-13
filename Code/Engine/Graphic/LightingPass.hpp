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
class GBuffer;
class BindlessResourceManager;
class D12Texture;
class D12Buffer;

/**
 * @brief LightingPass类 - 延迟光照计算系统
 * 
 * 教学要点:
 * 1. 基于Iris规范的延迟光照实现：仅处理不透明物体的光照计算
 * 2. 兼容Minecraft原版光照：在原版光照基础上添加现代光照效果
 * 3. 输出光照结果供半透明阶段使用：半透明物体在gbuffers_translucent阶段读取此结果作为背景
 * 4. 支持多种光照模型：PBR、Blinn-Phong等，可在运行时切换
 * 
 * Iris兼容性:
 * - 对应deferred1-99阶段：可以有多个延迟光照通道
 * - 读取G-Buffer RT0-RT3：获取几何属性数据
 * - 输出到临时RT：为后续阶段提供光照结果
 * - 不处理半透明：半透明物体在后续的gbuffers_*_translucent阶段处理
 * 
 * DirectX 12特性:
 * - 使用Bindless资源访问G-Buffer和Shadow Map
 * - 支持Compute Shader加速的光照计算
 * - 多线程光源剔除和分组
 * - GPU-Driven光照计算支持
 * 
 * @note 这是延迟渲染管线的核心，专注于高效的光照计算而非半透明混合
 */
class LightingPass final {
public:
    /**
     * @brief 光照模型枚举
     * 
     * 教学要点: 支持多种光照模型，可根据需求和性能要求选择
     */
    enum class LightingModel {
        MinecraftVanilla,   // 仅使用Minecraft原版光照
        BlinnPhong,         // 经典Blinn-Phong光照模型
        PBR,                // 基于物理的渲染 (PBR)
        Hybrid              // 混合模式：原版光照 + 现代技术
    };
    
    /**
     * @brief 光源类型枚举
     */
    enum class LightType {
        Directional,        // 方向光 (太阳、月亮)
        Point,              // 点光源 (火把、岩浆等)
        Spot,               // 聚光灯
        Area                // 面光源 (发光方块)
    };
    
    /**
     * @brief 延迟光照阶段枚举 (对应Iris的deferred1-99)
     */
    enum class DeferredStage {
        MainLighting = 0,   // deferred1: 主光照计算
        SSAO,              // deferred2: 屏幕空间环境光遮蔽
        Reflections,       // deferred3: 屏幕空间反射
        VolumetricFog,     // deferred4: 体积雾效
        Custom1,           // deferred5+: 自定义光照效果
        Custom2,
        Custom3,
        // ... 可扩展到deferred99
        MaxStages = 16     // 最多支持16个延迟阶段
    };

private:
    /**
     * @brief 光源数据结构 (GPU-friendly layout)
     * 
     * 教学要点: 设计为GPU友好的数据布局，避免padding问题
     */
    struct LightData {
        float position[3];      // 光源位置 (世界坐标)
        uint32_t type;          // 光源类型 (LightType)
        
        float direction[3];     // 光源方向 (方向光/聚光灯)
        float range;            // 光源范围
        
        float color[3];         // 光源颜色 (RGB)
        float intensity;        // 光源强度
        
        float spotInnerAngle;   // 聚光灯内角 (弧度)
        float spotOuterAngle;   // 聚光灯外角 (弧度)
        float padding[2];       // 对齐到64字节
        
        LightData();
    };
    
    /**
     * @brief 光照计算参数结构
     */
    struct LightingParams {
        float ambientColor[3];      // 环境光颜色
        float ambientIntensity;     // 环境光强度
        
        float sunDirection[3];      // 太阳方向
        float sunIntensity;         // 太阳强度
        
        float moonDirection[3];     // 月亮方向
        float moonIntensity;        // 月亮强度
        
        uint32_t lightingModel;     // 当前光照模型
        uint32_t enableSSAO;        // 是否启用SSAO
        uint32_t enableShadows;     // 是否启用阴影
        uint32_t padding;           // 对齐
        
        LightingParams();
    };
    
    // 核心资源
    ID3D12Device*               m_device;           // DX12设备 (外部管理)
    BindlessResourceManager*    m_bindlessManager;  // Bindless资源管理器 (外部管理)
    
    // 输出渲染目标 - 延迟光照结果
    std::shared_ptr<D12Texture> m_deferredLightingRT;    // 主光照结果 (RGBA16F)
    std::shared_ptr<D12Texture> m_ssaoRT;                // SSAO结果 (R8)
    std::shared_ptr<D12Texture> m_reflectionRT;          // 反射结果 (RGBA16F)
    std::shared_ptr<D12Texture> m_volumetricFogRT;       // 体积雾结果 (RGBA16F)
    
    // 光源管理
    std::shared_ptr<D12Buffer>  m_lightDataBuffer;      // 光源数据结构化缓冲
    std::shared_ptr<D12Buffer>  m_lightingParamsBuffer; // 光照参数常量缓冲
    std::vector<LightData>      m_lights;               // CPU端光源数组
    std::shared_ptr<D12Buffer>  m_lightIndicesBuffer;   // 光源索引缓冲 (用于剔除)
    
    // Shadow Map支持
    std::shared_ptr<D12Texture> m_shadowMap;           // 级联阴影贴图
    std::shared_ptr<D12Buffer>  m_shadowMatricesBuffer; // 阴影变换矩阵缓冲
    
    // Bindless索引
    uint32_t                    m_deferredLightingIndex;    // 主光照结果的Bindless索引
    uint32_t                    m_ssaoIndex;                // SSAO结果的Bindless索引
    uint32_t                    m_lightDataIndex;           // 光源数据的Bindless索引
    uint32_t                    m_shadowMapIndex;           // Shadow Map的Bindless索引
    
    // 配置参数
    LightingModel               m_currentModel;         // 当前光照模型
    LightingParams              m_lightingParams;       // 光照参数
    uint32_t                    m_renderWidth;          // 渲染宽度
    uint32_t                    m_renderHeight;         // 渲染高度
    uint32_t                    m_maxLights;            // 最大光源数量
    
    // 性能统计
    uint32_t                    m_activeLightCount;     // 当前活跃光源数量
    uint32_t                    m_culledLightCount;     // 被剔除的光源数量
    
    // 状态管理
    bool                        m_initialized;          // 初始化状态
    bool                        m_resourcesCreated;     // 资源创建状态
    bool                        m_lightDataDirty;       // 光源数据是否需要更新
    
    // 默认配置
    static constexpr uint32_t   DEFAULT_MAX_LIGHTS = 1024;     // 默认最大光源数量
    static constexpr float      DEFAULT_AMBIENT_INTENSITY = 0.1f; // 默认环境光强度

public:
    /**
     * @brief 构造函数
     * 
     * 教学要点: 基本初始化，实际资源创建在Initialize中
     */
    LightingPass();
    
    /**
     * @brief 析构函数
     * 
     * 教学要点: 智能指针自动处理资源释放，但需要从Bindless管理器注销
     */
    ~LightingPass();
    
    // ========================================================================
    // 生命周期管理
    // ========================================================================
    
    /**
     * @brief 初始化光照系统
     * @param device DX12设备指针
     * @param bindlessManager Bindless资源管理器
     * @param maxLights 最大光源数量
     * @return 成功返回true，失败返回false
     * 
     * 教学要点: 设置默认光照参数，准备资源创建
     */
    bool Initialize(ID3D12Device* device, BindlessResourceManager* bindlessManager, 
                   uint32_t maxLights = DEFAULT_MAX_LIGHTS);
    
    /**
     * @brief 创建光照渲染资源
     * @param width 渲染宽度
     * @param height 渲染高度
     * @return 成功返回true，失败返回false
     * 
     * 教学要点:
     * 1. 创建延迟光照输出RT (RGBA16F高精度)
     * 2. 创建SSAO、反射等辅助RT
     * 3. 创建光源数据和参数缓冲区
     * 4. 注册到Bindless管理器获取索引
     */
    bool CreateResources(uint32_t width, uint32_t height);
    
    /**
     * @brief 释放所有光照资源
     */
    void ReleaseResources();
    
    /**
     * @brief 重新创建资源 (分辨率变更)
     * @param newWidth 新宽度
     * @param newHeight 新高度
     * @return 成功返回true，失败返回false
     */
    bool RecreateResources(uint32_t newWidth, uint32_t newHeight);
    
    // ========================================================================
    // 延迟光照执行接口 (对应Iris deferred1-99阶段)
    // ========================================================================
    
    /**
     * @brief 执行主延迟光照计算 (deferred1)
     * @param commandList 命令列表
     * @param gBuffer G-Buffer管理器
     * 
     * 教学要点:
     * 1. 读取G-Buffer RT0-RT3获取几何属性
     * 2. 结合Minecraft原版光照数据
     * 3. 计算所有光源的贡献
     * 4. 输出到主光照RT供后续使用
     */
    void ExecuteMainLighting(ID3D12GraphicsCommandList* commandList, const GBuffer& gBuffer);
    
    /**
     * @brief 执行SSAO计算 (deferred2)
     * @param commandList 命令列表
     * @param gBuffer G-Buffer管理器 (需要深度和法线)
     * 
     * 教学要点: 屏幕空间环境光遮蔽，增强画面深度感
     */
    void ExecuteSSAO(ID3D12GraphicsCommandList* commandList, const GBuffer& gBuffer);
    
    /**
     * @brief 执行屏幕空间反射 (deferred3)
     * @param commandList 命令列表
     * @param gBuffer G-Buffer管理器
     * 
     * 教学要点: SSR增强金属材质的反射效果
     */
    void ExecuteScreenSpaceReflections(ID3D12GraphicsCommandList* commandList, const GBuffer& gBuffer);
    
    /**
     * @brief 执行体积雾效果 (deferred4)
     * @param commandList 命令列表
     * @param gBuffer G-Buffer管理器
     */
    void ExecuteVolumetricFog(ID3D12GraphicsCommandList* commandList, const GBuffer& gBuffer);
    
    /**
     * @brief 执行指定的延迟光照阶段
     * @param stage 延迟阶段枚举
     * @param commandList 命令列表
     * @param gBuffer G-Buffer管理器
     * @return 成功返回true，失败或跳过返回false
     * 
     * 教学要点: 统一的阶段执行接口，便于管理复杂的光照管线
     */
    bool ExecuteDeferredStage(DeferredStage stage, ID3D12GraphicsCommandList* commandList, const GBuffer& gBuffer);
    
    // ========================================================================
    // 前向渲染光照支持 (为gbuffers_*_translucent阶段提供)
    // ========================================================================
    
    /**
     * @brief 为前向渲染提供光照计算支持
     * @param commandList 命令列表
     * 
     * 教学要点:
     * 1. 绑定光源数据到着色器
     * 2. 设置光照参数常量缓冲
     * 3. 绑定Shadow Map用于阴影计算
     * 4. 半透明物体的着色器可以调用相同的光照函数
     */
    void PrepareForwardLightingResources(ID3D12GraphicsCommandList* commandList);
    
    /**
     * @brief 获取延迟光照结果RT (供半透明阶段作为背景使用)
     * @return 延迟光照结果纹理
     * 
     * 教学要点: 这是Iris架构的关键，半透明阶段读取此RT作为背景进行混合
     */
    std::shared_ptr<D12Texture> GetDeferredLightingResult() const { return m_deferredLightingRT; }
    
    /**
     * @brief 获取延迟光照结果的Bindless索引
     * @return Bindless索引，用于着色器访问
     */
    uint32_t GetDeferredLightingBindlessIndex() const { return m_deferredLightingIndex; }
    
    // ========================================================================
    // 光源管理接口
    // ========================================================================
    
    /**
     * @brief 添加点光源
     * @param position 光源位置 (世界坐标)
     * @param color 光源颜色 (RGB, 0-1)
     * @param intensity 光源强度
     * @param range 光源范围
     * @return 光源ID，失败返回UINT32_MAX
     * 
     * 教学要点: 动态光源管理，支持运行时添加/移除光源
     */
    uint32_t AddPointLight(const float position[3], const float color[3], float intensity, float range);
    
    /**
     * @brief 添加方向光 (太阳/月亮)
     * @param direction 光照方向 (世界坐标，需要归一化)
     * @param color 光源颜色
     * @param intensity 光源强度
     * @return 光源ID
     */
    uint32_t AddDirectionalLight(const float direction[3], const float color[3], float intensity);
    
    /**
     * @brief 添加聚光灯
     * @param position 光源位置
     * @param direction 光照方向
     * @param color 光源颜色
     * @param intensity 光源强度
     * @param range 光源范围
     * @param innerAngle 内锥角 (弧度)
     * @param outerAngle 外锥角 (弧度)
     * @return 光源ID
     */
    uint32_t AddSpotLight(const float position[3], const float direction[3], const float color[3],
                         float intensity, float range, float innerAngle, float outerAngle);
    
    /**
     * @brief 移除光源
     * @param lightId 光源ID
     * @return 成功返回true，失败返回false
     */
    bool RemoveLight(uint32_t lightId);
    
    /**
     * @brief 更新光源参数
     * @param lightId 光源ID
     * @param position 新位置 (可为nullptr表示不更新)
     * @param color 新颜色 (可为nullptr表示不更新)
     * @param intensity 新强度 (负值表示不更新)
     * @return 成功返回true，失败返回false
     */
    bool UpdateLight(uint32_t lightId, const float* position = nullptr, 
                    const float* color = nullptr, float intensity = -1.0f);
    
    /**
     * @brief 清除所有动态光源
     * 
     * 教学要点: 保留太阳/月亮等固定光源，清除动态添加的光源
     */
    void ClearDynamicLights();
    
    /**
     * @brief 更新光源数据到GPU
     * @param commandList 命令列表 (用于数据上传)
     * 
     * 教学要点: 将CPU端的光源修改同步到GPU缓冲区
     */
    void UpdateLightDataToGPU(ID3D12GraphicsCommandList* commandList);
    
    // ========================================================================
    // 光照模型和参数配置
    // ========================================================================
    
    /**
     * @brief 设置光照模型
     * @param model 光照模型类型
     * 
     * 教学要点: 运行时切换光照模型，对比不同效果
     */
    void SetLightingModel(LightingModel model) { m_currentModel = model; }
    
    /**
     * @brief 获取当前光照模型
     * @return 当前光照模型
     */
    LightingModel GetLightingModel() const { return m_currentModel; }
    
    /**
     * @brief 设置环境光参数
     * @param color 环境光颜色 (RGB)
     * @param intensity 环境光强度
     */
    void SetAmbientLighting(const float color[3], float intensity);
    
    /**
     * @brief 设置太阳光参数 (Minecraft昼夜循环)
     * @param direction 太阳方向 (世界坐标)
     * @param intensity 太阳强度 (0-1, 0为夜晚)
     */
    void SetSunLighting(const float direction[3], float intensity);
    
    /**
     * @brief 设置月亮光参数
     * @param direction 月亮方向
     * @param intensity 月亮强度
     */
    void SetMoonLighting(const float direction[3], float intensity);
    
    /**
     * @brief 启用/禁用SSAO
     * @param enabled 是否启用
     */
    void SetSSAOEnabled(bool enabled);
    
    /**
     * @brief 启用/禁用阴影
     * @param enabled 是否启用
     */
    void SetShadowsEnabled(bool enabled);
    
    // ========================================================================
    // Shadow Map集成
    // ========================================================================
    
    /**
     * @brief 设置Shadow Map纹理
     * @param shadowMap Shadow Map纹理
     * @param shadowMatrices 阴影变换矩阵缓冲
     * 
     * 教学要点: 集成外部的Shadow Map系统
     */
    void SetShadowMap(const std::shared_ptr<D12Texture>& shadowMap, 
                     const std::shared_ptr<D12Buffer>& shadowMatrices);
    
    /**
     * @brief 获取Shadow Map的Bindless索引
     * @return Shadow Map索引
     */
    uint32_t GetShadowMapBindlessIndex() const { return m_shadowMapIndex; }
    
    // ========================================================================
    // 查询接口
    // ========================================================================
    
    /**
     * @brief 获取当前活跃光源数量
     * @return 活跃光源数量
     */
    uint32_t GetActiveLightCount() const { return m_activeLightCount; }
    
    /**
     * @brief 获取最大支持的光源数量
     * @return 最大光源数量
     */
    uint32_t GetMaxLightCount() const { return m_maxLights; }
    
    /**
     * @brief 获取渲染分辨率
     * @param width 输出宽度
     * @param height 输出高度
     */
    void GetRenderResolution(uint32_t& width, uint32_t& height) const {
        width = m_renderWidth;
        height = m_renderHeight;
    }
    
    /**
     * @brief 检查是否已初始化
     * @return 已初始化返回true，否则返回false
     */
    bool IsInitialized() const { return m_initialized; }
    
    /**
     * @brief 检查资源是否已创建
     * @return 已创建返回true，否则返回false
     */
    bool AreResourcesCreated() const { return m_resourcesCreated; }
    
    /**
     * @brief 估算光照系统的显存占用
     * @return 显存大小 (字节)
     */
    size_t EstimateMemoryUsage() const;

private:
    // ========================================================================
    // 私有辅助方法
    // ========================================================================
    
    /**
     * @brief 初始化默认光照参数
     * 
     * 教学要点: 设置合理的默认光照环境
     */
    void InitializeDefaultParameters();
    
    /**
     * @brief 创建输出渲染目标
     * @param width RT宽度
     * @param height RT高度
     * @return 成功返回true，失败返回false
     */
    bool CreateOutputRenderTargets(uint32_t width, uint32_t height);
    
    /**
     * @brief 创建光源数据缓冲区
     * @return 成功返回true，失败返回false
     */
    bool CreateLightDataBuffers();
    
    /**
     * @brief 注册资源到Bindless管理器
     * @return 成功返回true，失败返回false
     */
    bool RegisterToBindlessManager();
    
    /**
     * @brief 从Bindless管理器注销资源
     */
    void UnregisterFromBindlessManager();
    
    /**
     * @brief 执行光源剔除 (移除不可见光源)
     * @param viewMatrix 视图矩阵
     * @param projMatrix 投影矩阵
     * @return 可见光源数量
     * 
     * 教学要点: GPU性能优化，只处理影响当前视图的光源
     */
    uint32_t CullLights(const float viewMatrix[16], const float projMatrix[16]);
    
    /**
     * @brief 分配新的光源ID
     * @return 光源ID，失败返回UINT32_MAX
     */
    uint32_t AllocateLightId();
    
    /**
     * @brief 获取延迟阶段的字符串名称 (调试用)
     * @param stage 延迟阶段
     * @return 阶段名称
     */
    static const char* GetDeferredStageName(DeferredStage stage);
    
    /**
     * @brief 获取光照模型的字符串名称 (调试用)
     * @param model 光照模型
     * @return 模型名称
     */
    static const char* GetLightingModelName(LightingModel model);
    
    // 禁用拷贝构造和赋值操作
    LightingPass(const LightingPass&) = delete;
    LightingPass& operator=(const LightingPass&) = delete;
    
    // 支持移动语义
    LightingPass(LightingPass&&) = default;
    LightingPass& operator=(LightingPass&&) = default;
};

} // namespace enigma::graphic