/**
 * @file PipelineManager.hpp
 * @brief Enigma管线管理器 - 基于Iris PipelineManager.java的DirectX 12实现
 * 
 * 教学重点:
 * 1. 理解Iris真实的管线管理架构模式
 * 2. 学习按维度（Dimension）分离管线的设计理念
 * 3. 掌握工厂模式在管线创建中的应用
 * 4. 理解管线生命周期管理和资源清理
 * 
 * 架构对应关系:
 * - 本类 ←→ net.irisshaders.iris.pipeline.PipelineManager.java
 * - 管理VanillaRenderingPipeline和EnigmaRenderingPipeline的切换
 * - 支持多维度管线缓存（主世界、下界、末地等）
 * - 提供统一的管线访问接口
 */

#pragma once

#include <memory>
#include <unordered_map>
#include <functional>
#include <string>
#include <optional>

#include "IWorldRenderingPipeline.hpp"

namespace enigma::graphic {

// 前向声明
class VanillaRenderingPipeline;
class EnigmaRenderingPipeline;
class ShaderPack;

/**
 * @brief 维度ID标识符
 * @details 
 * 对应Iris中的NamespacedId，用于区分不同的游戏维度
 * 每个维度可能需要不同的着色器程序和渲染设置
 * 
 * 教学要点:
 * - 理解为什么需要按维度分离渲染管线
 * - 学习命名空间ID的组织方式
 * - 掌握字符串标识符在渲染系统中的应用
 */
struct NamespacedId {
    std::string nameSpace;    ///< 命名空间（如"minecraft"）
    std::string path;         ///< 路径（如"overworld", "the_nether", "the_end"）
    
    /**
     * @brief 构造函数
     * @param ns 命名空间
     * @param p 路径
     */
    NamespacedId(const std::string& ns, const std::string& p) 
        : nameSpace(ns), path(p) {}
    
    /**
     * @brief 默认构造函数 - 主世界
     */
    NamespacedId() : nameSpace("minecraft"), path("overworld") {}
    
    /**
     * @brief 相等比较运算符
     */
    bool operator==(const NamespacedId& other) const {
        return nameSpace == other.nameSpace && path == other.path;
    }
    
    /**
     * @brief 转换为字符串表示
     */
    std::string ToString() const {
        return nameSpace + ":" + path;
    }
};

/**
 * @brief 维度ID标识符的哈希函数
 * @details 用于支持NamespacedId作为std::unordered_map的键
 */
struct NamespacedIdHash {
    std::size_t operator()(const NamespacedId& id) const {
        return std::hash<std::string>{}(id.nameSpace + ":" + id.path);
    }
};

/**
 * @brief 预定义的维度ID常量
 * @details 对应Iris中的DimensionId常量
 */
namespace DimensionIds {
    static const NamespacedId OVERWORLD("minecraft", "overworld");
    static const NamespacedId NETHER("minecraft", "the_nether");
    static const NamespacedId END("minecraft", "the_end");
}

/**
 * @brief 管线管理器 - 对应Iris PipelineManager.java
 * @details
 * 这个类是整个渲染管线系统的核心管理器，负责：
 * - 按维度管理不同的渲染管线实例
 * - 提供管线的创建、缓存和销毁功能
 * - 支持着色器包的动态加载和切换
 * - 管理管线的生命周期和资源清理
 * 
 * 教学要点:
 * - 理解为什么需要管线管理器而不是直接使用单个渲染器
 * - 学习工厂模式在复杂对象创建中的应用
 * - 掌握缓存策略在图形渲染中的重要性
 * - 理解多维度渲染的设计考量
 * 
 * Iris对应关系:
 * - pipelineFactory ←→ Function<NamespacedId, WorldRenderingPipeline> pipelineFactory
 * - pipelinesPerDimension ←→ Map<NamespacedId, WorldRenderingPipeline> pipelinesPerDimension
 * - currentPipeline ←→ WorldRenderingPipeline pipeline
 * - preparePipeline() ←→ preparePipeline(NamespacedId currentDimension)
 * 
 * @note 这是基于真实Iris源码的实现，确保架构的准确性
 */
class PipelineManager {
public:
    /**
     * @brief 管线创建工厂函数类型
     * @details 
     * 对应Iris中的Function<NamespacedId, WorldRenderingPipeline>
     * 用于根据维度ID创建相应的渲染管线实例
     * 
     * @param dimensionId 维度标识符
     * @return 创建的渲染管线智能指针
     */
    using PipelineFactory = std::function<std::unique_ptr<IWorldRenderingPipeline>(const NamespacedId&)>;

    /**
     * @brief 管线统计信息
     * @details 用于调试和性能分析的统计数据
     */
    struct Statistics {
        uint32_t activePipelines = 0;          ///< 活跃管线数量
        uint32_t totalCreatedPipelines = 0;    ///< 总共创建的管线数量
        uint32_t pipelineSwitches = 0;         ///< 管线切换次数
        std::string currentDimension;          ///< 当前维度名称
        
        /**
         * @brief 重置统计信息
         */
        void Reset() {
            activePipelines = 0;
            pipelineSwitches = 0;
        }
    };

public:
    /**
     * @brief 构造函数
     * @param factory 管线创建工厂函数
     * @details 
     * 对应Iris构造函数：
     * public PipelineManager(Function<NamespacedId, WorldRenderingPipeline> pipelineFactory)
     * 
     * 教学要点:
     * - 理解依赖注入模式在管线管理中的应用
     * - 学习工厂函数如何解耦管线创建逻辑
     */
    explicit PipelineManager(PipelineFactory factory);
    
    /**
     * @brief 析构函数
     * @details 确保所有管线资源得到正确释放
     */
    ~PipelineManager();

    // 禁用拷贝构造和赋值 - 管线管理器应该是唯一的
    PipelineManager(const PipelineManager&) = delete;
    PipelineManager& operator=(const PipelineManager&) = delete;
    
    // 启用移动构造和赋值 - 支持高效的资源转移
    PipelineManager(PipelineManager&&) noexcept = default;
    PipelineManager& operator=(PipelineManager&&) noexcept = default;

    // ==================== 核心管线管理接口 ====================
    
    /**
     * @brief 准备并获取指定维度的渲染管线
     * @param currentDimension 当前维度ID
     * @return 渲染管线指针，如果创建失败返回nullptr
     * @details 
     * 对应Iris方法：
     * public WorldRenderingPipeline preparePipeline(NamespacedId currentDimension)
     * 
     * 核心逻辑:
     * 1. 检查是否已存在该维度的管线缓存
     * 2. 如果不存在，调用工厂函数创建新管线
     * 3. 将新管线添加到缓存中
     * 4. 返回管线指针供渲染系统使用
     * 
     * 教学要点:
     * - 理解延迟创建（lazy creation）策略
     * - 学习缓存在减少创建开销中的作用
     * - 掌握维度切换时的管线管理逻辑
     */
    IWorldRenderingPipeline* PreparePipeline(const NamespacedId& currentDimension);
    
    /**
     * @brief 获取当前活跃的管线（可能为nullptr）
     * @return 当前管线指针，可能为nullptr
     * @details 
     * 对应Iris方法：@Nullable public WorldRenderingPipeline getPipelineNullable()
     * 
     * 用途:
     * - 快速访问当前管线而不触发创建
     * - 检查是否有管线处于活跃状态
     * - 在某些渲染决策中判断管线可用性
     */
    IWorldRenderingPipeline* GetPipelineNullable() const noexcept { return m_currentPipeline.get(); }
    
    /**
     * @brief 获取当前活跃的管线（Optional包装）
     * @return Optional包装的管线指针
     * @details 
     * 对应Iris方法：public Optional<WorldRenderingPipeline> getPipeline()
     * 
     * 提供类型安全的管线访问方式，避免空指针访问
     */
    std::optional<IWorldRenderingPipeline*> GetPipeline() const noexcept {
        return m_currentPipeline ? std::optional<IWorldRenderingPipeline*>{m_currentPipeline.get()} 
                                 : std::nullopt;
    }

    // ==================== 管线生命周期管理 ====================
    
    /**
     * @brief 销毁所有管线
     * @details 
     * 对应Iris方法：public void destroyPipeline()
     * 
     * ⚠️ 危险操作！这个方法会销毁所有当前的管线实例。
     * 必须确保在销毁后立即重新准备管线，以防止程序进入不一致状态。
     * 
     * 使用场景:
     * - 着色器包重新加载
     * - 图形设备重置
     * - 渲染设置大幅变更
     * 
     * 教学要点:
     * - 理解为什么需要完全销毁和重建管线
     * - 学习资源管理中的"原子性"概念
     * - 掌握危险操作的安全使用模式
     */
    void DestroyAllPipelines();
    
    /**
     * @brief 销毁特定维度的管线
     * @param dimensionId 要销毁的维度ID
     * @details 更细粒度的管线销毁，只影响指定维度
     */
    void DestroyPipeline(const NamespacedId& dimensionId);
    
    /**
     * @brief 检查指定维度是否有缓存的管线
     * @param dimensionId 维度ID
     * @return true表示存在缓存的管线
     */
    bool HasCachedPipeline(const NamespacedId& dimensionId) const noexcept;
    
    /**
     * @brief 获取当前缓存的管线数量
     * @return 缓存的管线数量
     */
    size_t GetCachedPipelineCount() const noexcept { return m_pipelinesPerDimension.size(); }

    // ==================== Sodium兼容性支持 ====================
    
    /**
     * @brief 获取Sodium着色器重载版本计数器
     * @return 版本计数器值
     * @details 
     * 对应Iris方法：public int getVersionCounterForSodiumShaderReload()
     * 
     * 用于解决与Immersive Portals等Mod的兼容性问题。
     * 每次销毁管线时递增，确保Sodium着色器正确重新加载。
     * 
     * 教学要点:
     * - 理解Mod兼容性在渲染系统中的重要性
     * - 学习版本计数器在状态同步中的应用
     * - 掌握第三方集成的设计考量
     */
    int GetVersionCounterForSodiumShaderReload() const noexcept { 
        return m_versionCounterForSodiumShaderReload; 
    }

    // ==================== 调试和统计接口 ====================
    
    /**
     * @brief 获取管线管理统计信息
     * @return 统计信息结构体
     * @details 用于性能分析和调试
     */
    const Statistics& GetStatistics() const noexcept { return m_statistics; }
    
    /**
     * @brief 获取所有已缓存维度的列表
     * @return 维度ID列表
     * @details 用于调试和管理界面显示
     */
    std::vector<NamespacedId> GetCachedDimensions() const;
    
    /**
     * @brief 获取当前活跃维度ID
     * @return 当前维度ID，如果没有活跃管线则返回默认值
     */
    NamespacedId GetCurrentDimension() const noexcept { return m_currentDimension; }

private:
    // ==================== 内部辅助方法 ====================
    
    /**
     * @brief 重置纹理状态
     * @details 
     * 对应Iris方法：private void resetTextureState()
     * 
     * 在销毁管线时解绑所有纹理，防止销毁的渲染目标纹理
     * 仍然绑定到特定纹理单元上，避免重载着色器包时出现问题。
     */
    void ResetTextureState();
    
    /**
     * @brief 更新统计信息
     */
    void UpdateStatistics();
    
    /**
     * @brief 记录维度切换日志
     */
    void LogDimensionSwitch(const NamespacedId& from, const NamespacedId& to);

private:
    // ==================== 核心成员变量 ====================
    
    /// 管线创建工厂函数 - 对应Iris的pipelineFactory
    PipelineFactory m_pipelineFactory;
    
    /// 按维度缓存的管线映射 - 对应Iris的pipelinesPerDimension
    std::unordered_map<NamespacedId, std::unique_ptr<IWorldRenderingPipeline>, NamespacedIdHash> m_pipelinesPerDimension;
    
    /// 当前活跃的管线 - 对应Iris的pipeline
    /// 注意：这是对m_pipelinesPerDimension中某个管线的引用，不拥有所有权
    std::unique_ptr<IWorldRenderingPipeline> m_currentPipeline;
    
    /// 当前维度ID
    NamespacedId m_currentDimension;
    
    /// Sodium着色器重载版本计数器 - 对应Iris的versionCounterForSodiumShaderReload
    int m_versionCounterForSodiumShaderReload = 0;
    
    /// 统计信息
    mutable Statistics m_statistics;
    
    /// 初始化状态标志
    bool m_isInitialized = false;
};

} // namespace enigma::graphic