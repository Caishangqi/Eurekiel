#pragma once

#include <memory>
#include <vector>
#include <string>
#include <d3d12.h>
#include <dxgi1_6.h>

// 前向声明
namespace enigma::graphic {
    class GBuffer;
    class BindlessResourceManager;
    class LightingPass;
    class ShaderPackManager;
    class CommandListManager;
}

namespace enigma::graphic {

/**
 * @brief EnigmaRenderer类 - DirectX 12延迟渲染引擎核心
 * 
 * 教学要点:
 * 1. 这是整个渲染系统的协调中心，管理完整的Iris兼容渲染管线
 * 2. 实现10阶段Iris渲染流程：setup → begin → shadow → shadowcomp → prepare → gbuffers(opaque) → deferred → gbuffers(translucent) → composite → final
 * 3. 使用智能指针管理所有子系统，确保资源的正确释放
 * 
 * DirectX 12特性:
 * - 支持Bindless资源绑定，减少描述符切换开销
 * - 多命令列表并行录制，提升CPU效率
 * - 精确的资源状态管理和内存屏障
 * - GPU-Driven渲染支持
 * 
 * Iris兼容性:
 * - 支持18种gbuffers程序类型的回退机制
 * - 8-10个可配置渲染目标
 * - Ping-Pong缓冲机制用于后处理链
 * - HLSL着色器替代GLSL，保持语义兼容
 * 
 * @note 这是教学项目的核心类，重点理解渲染管线的整体架构
 */
class EnigmaRenderer final {
private:
    // DirectX 12核心资源
    ID3D12Device*              m_device;           // D3D12设备 (由外部管理)
    ID3D12CommandQueue*        m_commandQueue;     // 主命令队列
    std::unique_ptr<CommandListManager> m_commandListManager; // 命令列表管理器
    
    // 渲染子系统 - 使用智能指针确保正确的生命周期管理
    std::unique_ptr<GBuffer>                m_gBuffer;              // G-Buffer管理器
    std::unique_ptr<BindlessResourceManager> m_bindlessManager;      // Bindless资源管理
    std::unique_ptr<LightingPass>           m_lightingPass;         // 延迟光照计算
    std::unique_ptr<ShaderPackManager>      m_shaderPackManager;    // Shader Pack管理
    
    // 渲染配置
    uint32_t m_renderWidth;     // 渲染分辨率宽度
    uint32_t m_renderHeight;    // 渲染分辨率高度
    bool     m_initialized;     // 初始化状态标记
    
    // Iris管线状态追踪
    enum class PipelineStage : uint32_t {
        Setup = 0,        // setup1-99 (Compute-only)
        Begin,            // begin1-99 (Composite-style)
        Shadow,           // shadow (Gbuffers-style)
        ShadowComp,       // shadowcomp1-99 (Composite-style)
        Prepare,          // prepare1-99 (Composite-style)  
        GBuffersOpaque,   // gbuffers_* opaque (Gbuffers-style)
        Deferred,         // deferred1-99 (Composite-style)
        GBuffersTranslucent, // gbuffers_* translucent (Gbuffers-style)
        Composite,        // composite1-99 (Composite-style)
        Final             // final (Composite-style)
    };
    
    PipelineStage m_currentStage; // 当前管线执行阶段
    
public:
    /**
     * @brief 构造函数
     * 
     * 教学要点: 构造函数只进行基本的成员初始化，实际的资源创建在Initialize()中进行
     * 这遵循了"两阶段初始化"模式，避免构造函数中的复杂操作和异常处理
     */
    EnigmaRenderer();
    
    /**
     * @brief 析构函数
     * 
     * 教学要点: 使用智能指针后，析构函数会自动调用子系统的析构
     * 但显式调用Shutdown()确保资源按正确顺序释放
     */
    ~EnigmaRenderer();
    
    // ========================================================================
    // 生命周期管理接口
    // ========================================================================
    
    /**
     * @brief 初始化渲染器
     * @param device DirectX 12设备指针 (由RendererSubsystem传入)
     * @param commandQueue 主命令队列指针
     * @param width 初始渲染宽度
     * @param height 初始渲染高度
     * 
     * 教学要点:
     * 1. 创建所有子系统实例 (使用make_unique确保异常安全)
     * 2. 初始化Bindless描述符堆
     * 3. 创建默认的渲染资源
     * 
     * @return 成功返回true，失败返回false
     */
    bool Initialize(ID3D12Device* device, ID3D12CommandQueue* commandQueue, uint32_t width, uint32_t height);
    
    /**
     * @brief 创建渲染资源
     * @param width 新的渲染宽度
     * @param height 新的渲染高度
     * 
     * 教学要点: 支持运行时分辨率变更，需要重新创建所有依赖分辨率的资源
     */
    void CreateRenderResources(uint32_t width, uint32_t height);
    
    /**
     * @brief 释放所有资源
     * 
     * 教学要点: 按照依赖关系的逆序释放资源，避免悬空指针
     */
    void Shutdown();
    
    // ========================================================================
    // Iris渲染管线执行接口 - 按照Iris官方执行顺序
    // ========================================================================
    
    /**
     * @brief 执行Setup阶段 (setup1-99)
     * 
     * 教学要点:
     * 1. 这是每帧的第一个阶段，仅支持Compute Shader
     * 2. 用于GPU状态初始化、全局缓冲区设置、异步计算任务启动
     * 3. 可以有多个setup程序按数字顺序执行
     * 
     * Iris规范: Compute-only programs，用于GPU状态初始化
     */
    void ExecuteSetupStage();
    
    /**
     * @brief 执行Begin阶段 (begin1-99)
     * 
     * 教学要点:
     * 1. Composite-style渲染，每帧参数更新
     * 2. 更新时间、摄像机矩阵、光照参数等全局uniform
     * 3. 为后续渲染阶段准备数据
     * 
     * Iris规范: Composite-style programs，全屏四边形渲染
     */
    void ExecuteBeginStage();
    
    /**
     * @brief 执行Shadow阶段 (shadow)
     * 
     * 教学要点:
     * 1. 从光源视角渲染深度图，生成Shadow Map
     * 2. 渲染所有几何体到深度缓冲区
     * 3. 支持Cascade Shadow Maps和多光源阴影
     * 
     * Iris规范: Gbuffers-style program，渲染几何体到Shadow Map
     */
    void ExecuteShadowStage();
    
    /**
     * @brief 执行ShadowComp阶段 (shadowcomp1-99)
     * 
     * 教学要点:
     * 1. Shadow Map后处理，软阴影计算、阴影过滤
     * 2. 可以有多个shadowcomp程序进行复杂的阴影效果处理
     * 3. 全屏后处理方式，输入Shadow Map，输出处理后的阴影数据
     * 
     * Iris规范: Composite-style programs，Shadow Map后处理
     */
    void ExecuteShadowCompStage();
    
    /**
     * @brief 执行Prepare阶段 (prepare1-99)
     * 
     * 教学要点:
     * 1. 屏幕空间环境光遮蔽 (SSAO) 预计算
     * 2. 深度预处理、法线重建等准备工作
     * 3. 为后续的延迟渲染阶段准备必要的数据
     * 
     * Iris规范: Composite-style programs，渲染前的预处理
     */
    void ExecutePrepareStage();
    
    /**
     * @brief 执行不透明几何体渲染阶段 (gbuffers_*)
     * 
     * 教学要点:
     * 1. 这是延迟渲染的核心阶段，将几何体属性写入G-Buffer
     * 2. 支持18种不同的gbuffers程序，按fallback顺序执行
     * 3. 输出到多个渲染目标 (MRT): Albedo, Normal, Material Properties等
     * 4. 仅处理不透明物体，半透明物体在后续阶段处理
     * 
     * Iris规范: Gbuffers-style programs，输出几何属性到MRT
     * 
     * 支持的程序类型:
     * - gbuffers_terrain: 地形渲染
     * - gbuffers_entities: 实体渲染  
     * - gbuffers_basic: 基础几何体
     * - 等等...共18种类型
     */
    void ExecuteGBuffersOpaqueStage();
    
    /**
     * @brief 执行延迟光照阶段 (deferred1-99)
     * 
     * 教学要点:
     * 1. 基于G-Buffer进行延迟光照计算
     * 2. 读取G-Buffer中的几何属性，计算最终光照
     * 3. 支持PBR、Blinn-Phong等多种光照模型
     * 4. 可以有多个deferred程序进行不同类型的光照计算
     * 
     * Iris规范: Composite-style programs，全屏光照计算
     */
    void ExecuteDeferredStage();
    
    /**
     * @brief 执行半透明几何体渲染阶段 (gbuffers_*_translucent)
     * 
     * 教学要点:
     * 1. 前向渲染半透明物体 (水、玻璃、粒子等)
     * 2. 与延迟光照结果进行Alpha混合
     * 3. 需要正确的深度排序以确保透明度效果
     * 
     * Iris规范: Gbuffers-style programs，前向渲染半透明物体
     * 
     * 支持的程序类型:
     * - gbuffers_water: 水面渲染
     * - gbuffers_hand_water: 手持水面
     * - gbuffers_entities_translucent: 半透明实体
     */
    void ExecuteGBuffersTranslucentStage();
    
    /**
     * @brief 执行后处理阶段 (composite1-99)
     * 
     * 教学要点:
     * 1. 后处理效果链: Bloom、Tone Mapping、Color Grading等
     * 2. Ping-Pong缓冲机制，顺序执行多个后处理通道
     * 3. 每个composite程序读取前一程序的输出，写入不同的缓冲区
     * 
     * Iris规范: Composite-style programs，按数字顺序执行的后处理链
     * 
     * 常见效果:
     * - composite1: Bloom提取
     * - composite2: Bloom模糊
     * - composite3: Tone Mapping
     * - composite4: Color Grading
     */
    void ExecuteCompositeStage();
    
    /**
     * @brief 执行最终输出阶段 (final)
     * 
     * 教学要点:
     * 1. 最终输出到屏幕Back Buffer
     * 2. 应用最终的gamma校正、对比度调整等
     * 3. 这是渲染管线的最后一个阶段
     * 
     * Iris规范: Composite-style program，输出到屏幕
     */
    void ExecuteFinalStage();
    
    // ========================================================================
    // 子系统访问接口
    // ========================================================================
    
    /**
     * @brief 获取G-Buffer管理器
     * @return G-Buffer管理器的shared_ptr，允许多个系统访问
     * 
     * 教学要点: 返回shared_ptr允许其他系统安全地访问G-Buffer
     */
    std::shared_ptr<GBuffer> GetGBuffer() const { return m_gBuffer; }
    
    /**
     * @brief 获取Bindless资源管理器
     * @return Bindless资源管理器的shared_ptr
     * 
     * 教学要点: Bindless管理器被多个渲染阶段共享使用
     */
    std::shared_ptr<BindlessResourceManager> GetBindlessManager() const { return m_bindlessManager; }
    
    /**
     * @brief 获取光照计算管理器
     * @return 光照计算管理器的shared_ptr
     */
    std::shared_ptr<LightingPass> GetLightingPass() const { return m_lightingPass; }
    
    /**
     * @brief 获取Shader Pack管理器
     * @return Shader Pack管理器的shared_ptr
     * 
     * 教学要点: ShaderPack管理器负责加载和管理HLSL着色器程序
     */
    std::shared_ptr<ShaderPackManager> GetShaderPackManager() const { return m_shaderPackManager; }
    
    // ========================================================================
    // 状态查询接口
    // ========================================================================
    
    /**
     * @brief 检查渲染器是否已初始化
     * @return 已初始化返回true，否则返回false
     */
    bool IsInitialized() const { return m_initialized; }
    
    /**
     * @brief 获取当前渲染分辨率
     * @param width 输出宽度
     * @param height 输出高度
     */
    void GetRenderResolution(uint32_t& width, uint32_t& height) const {
        width = m_renderWidth;
        height = m_renderHeight;
    }
    
    /**
     * @brief 获取当前管线执行阶段
     * @return 当前执行阶段枚举值
     * 
     * 教学要点: 用于调试和性能分析，了解当前渲染进度
     */
    PipelineStage GetCurrentStage() const { return m_currentStage; }
    
    /**
     * @brief 获取阶段名称字符串 (用于调试)
     * @param stage 管线阶段枚举值
     * @return 阶段名称字符串
     */
    static const char* GetStageName(PipelineStage stage);

private:
    // ========================================================================
    // 私有辅助方法
    // ========================================================================
    
    /**
     * @brief 初始化所有子系统
     * 
     * 教学要点: 按照依赖关系顺序初始化子系统
     * 1. 首先初始化基础的Bindless资源管理
     * 2. 然后初始化G-Buffer和光照系统
     * 3. 最后初始化Shader Pack管理系统
     */
    bool InitializeSubSystems();
    
    /**
     * @brief 设置当前管线阶段并记录调试信息
     * @param stage 要设置的管线阶段
     * 
     * 教学要点: 统一的阶段切换接口，便于调试和性能监控
     */
    void SetCurrentStage(PipelineStage stage);
    
    /**
     * @brief 验证管线执行顺序的正确性
     * @param nextStage 下一个要执行的阶段
     * @return 顺序正确返回true，否则返回false
     * 
     * 教学要点: 确保严格按照Iris规范的执行顺序进行渲染
     */
    bool ValidatePipelineOrder(PipelineStage nextStage) const;
    
    // 禁用拷贝构造和赋值操作
    EnigmaRenderer(const EnigmaRenderer&) = delete;
    EnigmaRenderer& operator=(const EnigmaRenderer&) = delete;
    
    // 支持移动语义 (用于智能指针管理)
    EnigmaRenderer(EnigmaRenderer&&) = default;
    EnigmaRenderer& operator=(EnigmaRenderer&&) = default;
};

} // namespace enigma::graphic