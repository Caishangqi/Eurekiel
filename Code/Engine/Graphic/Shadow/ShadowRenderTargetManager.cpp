/**
 * @file ShadowRenderTargetManager.cpp
 * @brief Iris兼容的阴影渲染目标管理器实现
 * @date 2025-10-11
 *
 * 对应Iris: ShadowRenderTargets.java (379行)
 * 核心职责:
 * 1. 懒加载创建8个shadowcolor RenderTarget
 * 2. 立即创建2个shadowtex深度纹理
 * 3. 管理Main/Alt翻转状态（BufferFlipState<8>）
 * 4. 提供Bindless索引查询接口
 */

#include "ShadowRenderTargetManager.hpp"
#include "Engine/Graphic/Target/D12RenderTarget.hpp"
#include "Engine/Graphic/Target/D12DepthTexture.hpp"
#include "Engine/Graphic/Shader/ShaderPack/Properties/PackShadowDirectives.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include <format>
#include <sstream>

namespace enigma::graphic
{
    // ============================================================================
    // 构造函数
    // ============================================================================

    /**
     * @brief 构造函数 - 创建shadowtex0/1并初始化配置
     *
     * Iris对应代码 (ShadowRenderTargets.java:52-75):
     * ```java
     * public ShadowRenderTargets(WorldRenderingPipeline pipeline, int resolution, PackShadowDirectives shadowDirectives) {
     *     this.resolution = resolution;
     *     this.shadowDirectives = shadowDirectives;
     *     this.mainDepth = new DepthTexture("shadowtex0", resolution, resolution, DepthBufferFormat.DEPTH);
     *     this.noTranslucents = new DepthTexture("shadowtex1", resolution, resolution, DepthBufferFormat.DEPTH);
     *     // targets数组懒加载，不在此初始化
     * }
     * ```
     *
     * 教学要点:
     * 1. shadowtex0/1立即创建（深度纹理必需）
     * 2. shadowcolor0-7懒加载（节省GPU内存）
     * 3. 固定分辨率，不受窗口影响
     * 4. 从PackShadowDirectives读取配置并缓存到m_settings
     */
    ShadowRenderTargetManager::ShadowRenderTargetManager(
        int                         resolution,
        const PackShadowDirectives& shadowDirectives
    )
        : m_resolution(resolution)
          , m_shadowDirectives(shadowDirectives)
    {
        // ========================================================================
        // 步骤1: 创建shadowtex0（主深度缓冲）
        // ========================================================================

        DepthTextureCreateInfo shadowTex0Info(
            "shadowtex0", // name
            resolution, resolution, // width, height
            DepthType::ShadowMap, // type (支持采样)
            1.0f, 0 // clearDepth, clearStencil
        );

        m_shadowTex0 = std::make_shared<D12DepthTexture>(shadowTex0Info);

        // ========================================================================
        // 步骤2: 创建shadowtex1（半透明前深度）
        // ========================================================================

        DepthTextureCreateInfo shadowTex1Info(
            "shadowtex1", // name
            resolution, resolution, // width, height
            DepthType::ShadowMap, // type (支持采样)
            1.0f, 0 // clearDepth, clearStencil
        );

        m_shadowTex1 = std::make_shared<D12DepthTexture>(shadowTex1Info);

        // ========================================================================
        // 步骤3: 初始化shadowcolor配置缓存（从PackShadowDirectives）
        // ========================================================================

        for (int i = 0; i < 8; ++i)
        {
            m_settings[i].format           = shadowDirectives.GetShadowColorFormat(i);
            m_settings[i].hardwareFiltered = shadowDirectives.IsShadowColorHardwareFiltered(i);
            m_settings[i].enableMipmap     = shadowDirectives.IsShadowColorMipmapEnabled(i);
            m_settings[i].linearFilter     = true; // 默认线性过滤
            m_settings[i].clearEveryFrame  = shadowDirectives.ShouldShadowColorClearEveryFrame(i);
            m_settings[i].debugName        = ("shadowcolor" + std::to_string(i)).c_str();
        }

        // ========================================================================
        // 步骤4: 初始化shadowcolor数组为nullptr（懒加载模式）
        // ========================================================================

        for (auto& shadowColor : m_shadowColorTargets)
        {
            shadowColor = nullptr; // 懒加载：首次GetOrCreate时创建
        }

        // ========================================================================
        // 步骤5: 重置翻转状态（所有shadowcolor初始为读Main写Alt）
        // ========================================================================

        m_flipState.Reset(); // BufferFlipState<8>::Reset()
    }

    // ============================================================================
    // RenderTarget访问接口 - 懒加载模式
    // ============================================================================

    /**
     * @brief 获取指定shadowcolor RT（如果不存在返回nullptr）
     *
     * Iris对应代码 (ShadowRenderTargets.java:94-96):
     * ```java
     * public RenderTarget get(int index) {
     *     return targets[index];
     * }
     * ```
     */
    std::shared_ptr<D12RenderTarget> ShadowRenderTargetManager::Get(int index) const
    {
        if (!IsValidIndex(index))
        {
            return nullptr;
        }

        return m_shadowColorTargets[index]; // 可能为nullptr
    }

    /**
     * @brief 获取或创建指定shadowcolor RT（懒加载核心方法）
     *
     * Iris对应代码 (ShadowRenderTargets.java:117-124):
     * ```java
     * public RenderTarget getOrCreate(int index) {
     *     if (targets[index] != null) {
     *         return targets[index];
     *     }
     *     create(index);
     *     return targets[index];
     * }
     * ```
     *
     * 教学要点:
     * 1. 懒加载模式：首次调用时才创建纹理
     * 2. 节省GPU内存（只创建实际使用的shadowcolor）
     * 3. 自动注册Bindless索引
     */
    std::shared_ptr<D12RenderTarget> ShadowRenderTargetManager::GetOrCreate(int index)
    {
        if (!IsValidIndex(index))
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetOrCreate() - Invalid index: " + std::to_string(index));
        }

        // 如果已存在，直接返回
        if (m_shadowColorTargets[index] != nullptr)
        {
            return m_shadowColorTargets[index];
        }

        // 否则创建新的shadowcolor RT
        CreateShadowColorRT(index);

        return m_shadowColorTargets[index];
    }

    /**
     * @brief 如果RT不存在则创建（辅助方法）
     *
     * Iris对应代码 (ShadowRenderTargets.java:139-144):
     * ```java
     * public void createIfEmpty(int index) {
     *     if (targets[index] == null) {
     *         create(index);
     *     }
     * }
     * ```
     */
    void ShadowRenderTargetManager::CreateIfEmpty(int index)
    {
        if (!IsValidIndex(index))
        {
            return;
        }

        if (m_shadowColorTargets[index] == nullptr)
        {
            CreateShadowColorRT(index);
        }
    }

    // ============================================================================
    // RTV访问接口 - 用于OMSetRenderTargets()
    // ============================================================================

    /**
     * @brief 获取指定shadowcolor的主RTV句柄
     *
     * 教学要点:
     * - 调用D12RenderTarget::GetMainRTV()获取RTV句柄
     * - RTV用于SetRenderTargets()绑定
     * - 自动触发懒加载（GetOrCreate）
     */
    D3D12_CPU_DESCRIPTOR_HANDLE ShadowRenderTargetManager::GetMainRTV(int index) const
    {
        if (!IsValidIndex(index))
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetMainRTV() - Invalid index: " + std::to_string(index));
        }

        auto rt = m_shadowColorTargets[index];
        if (rt == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetMainRTV() - shadowcolor" + std::to_string(index) + " not created yet. Call GetOrCreate() first.");
        }

        return rt->GetMainRTV();
    }

    /**
     * @brief 获取指定shadowcolor的替代RTV句柄
     */
    D3D12_CPU_DESCRIPTOR_HANDLE ShadowRenderTargetManager::GetAltRTV(int index) const
    {
        if (!IsValidIndex(index))
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetAltRTV() - Invalid index: " + std::to_string(index));
        }

        auto rt = m_shadowColorTargets[index];
        if (rt == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetAltRTV() - shadowcolor" + std::to_string(index) + " not created yet. Call GetOrCreate() first.");
        }

        return rt->GetAltRTV();
    }

    /**
     * @brief 获取shadowtex0的DSV句柄
     */
    D3D12_CPU_DESCRIPTOR_HANDLE ShadowRenderTargetManager::GetShadowTex0DSV() const
    {
        if (m_shadowTex0 == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetShadowTex0DSV() - shadowtex0 is nullptr");
        }

        return m_shadowTex0->GetDSVHandle();
    }

    /**
     * @brief 获取shadowtex1的DSV句柄
     */
    D3D12_CPU_DESCRIPTOR_HANDLE ShadowRenderTargetManager::GetShadowTex1DSV() const
    {
        if (m_shadowTex1 == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetShadowTex1DSV() - shadowtex1 is nullptr");
        }

        return m_shadowTex1->GetDSVHandle();
    }

    // ============================================================================
    // Bindless索引访问 - 用于着色器ResourceDescriptorHeap访问
    // ============================================================================

    /**
     * @brief 获取指定shadowcolor的Main纹理Bindless索引
     *
     * Iris对应代码 (ShadowRenderTargets.java:151-153):
     * ```java
     * public int getColorTextureId(int i) {
     *     return get(i).getMainTexture();
     * }
     * ```
     *
     * 教学要点:
     * - 着色器通过此索引访问: ResourceDescriptorHeap[index]
     * - Bindless索引在纹理创建时注册，不会频繁变更
     * - 零Root Signature切换开销
     */
    uint32_t ShadowRenderTargetManager::GetMainTextureIndex(int index) const
    {
        if (!IsValidIndex(index))
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetMainTextureIndex() - Invalid index: " + std::to_string(index));
        }

        auto rt = m_shadowColorTargets[index];
        if (rt == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetMainTextureIndex() - shadowcolor" + std::to_string(index) + " not created yet. Call GetOrCreate() first.");
        }

        return rt->GetMainTextureIndex();
    }

    /**
     * @brief 获取指定shadowcolor的Alt纹理Bindless索引
     */
    uint32_t ShadowRenderTargetManager::GetAltTextureIndex(int index) const
    {
        if (!IsValidIndex(index))
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetAltTextureIndex() - Invalid index: " + std::to_string(index));
        }

        auto rt = m_shadowColorTargets[index];
        if (rt == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetAltTextureIndex() - shadowcolor" + std::to_string(index) + " not created yet. Call GetOrCreate() first.");
        }

        return rt->GetAltTextureIndex();
    }

    /**
     * @brief 获取shadowtex0的Bindless索引
     *
     * Iris对应代码 (ShadowRenderTargets.java:171-173):
     * ```java
     * public int getDepthTextureId() {
     *     return mainDepth.getTextureId();
     * }
     * ```
     */
    uint32_t ShadowRenderTargetManager::GetShadowTex0Index() const
    {
        if (m_shadowTex0 == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetShadowTex0Index() - shadowtex0 is nullptr");
        }

        return m_shadowTex0->GetBindlessIndex();
    }

    /**
     * @brief 获取shadowtex1的Bindless索引
     *
     * Iris对应代码 (ShadowRenderTargets.java:175-177):
     * ```java
     * public int getDepthTextureNoTranslucentsId() {
     *     return noTranslucents.getTextureId();
     * }
     * ```
     */
    uint32_t ShadowRenderTargetManager::GetShadowTex1Index() const
    {
        if (m_shadowTex1 == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::GetShadowTex1Index() - shadowtex1 is nullptr");
        }

        return m_shadowTex1->GetBindlessIndex();
    }

    // ============================================================================
    // 查询接口
    // ============================================================================

    /**
     * @brief 检查指定shadowcolor是否启用硬件过滤
     *
     * Iris对应代码 (ShadowRenderTargets.java:204-206):
     * ```java
     * public boolean isHardwareFiltered(int i) {
     *     return hardwareFiltered[i];
     * }
     * ```
     */
    bool ShadowRenderTargetManager::IsHardwareFiltered(int index) const
    {
        if (!IsValidIndex(index))
        {
            return false;
        }

        return m_settings[index].hardwareFiltered;
    }

    // ============================================================================
    // 私有方法 - CreateShadowColorRT
    // ============================================================================

    /**
     * @brief 创建指定shadowcolor RT（内部辅助函数）
     *
     * Iris对应代码 (ShadowRenderTargets.java:126-137):
     * ```java
     * private void create(int index) {
     *     targets[index] = RenderTarget.builder()
     *         .setName("shadowcolor" + index)
     *         .setInternalFormat(formats[index])
     *         .setPixelFormat(PixelFormat.fromInternalFormat(formats[index]))
     *         .setDimensions(resolution, resolution)
     *         .build();
     * }
     * ```
     *
     * 教学要点:
     * 1. 使用Builder模式创建D12RenderTarget
     * 2. 从m_settings[index]读取配置
     * 3. 自动注册Bindless索引
     * 4. 设置调试名称（shadowcolorN）
     */
    void ShadowRenderTargetManager::CreateShadowColorRT(int index)
    {
        if (!IsValidIndex(index))
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::CreateShadowColorRT() - Invalid index: " + std::to_string(index));
        }

        // 使用Builder模式创建RenderTarget
        m_shadowColorTargets[index] = D12RenderTarget::Create()
                                      .SetName(std::string("shadowcolor") + std::to_string(index))
                                      .SetFormat(m_settings[index].format)
                                      .SetDimensions(m_resolution, m_resolution)
                                      .SetLinearFilter(m_settings[index].linearFilter)
                                      .EnableMipmap(m_settings[index].enableMipmap)
                                      .Build();

        // 验证创建成功
        if (m_shadowColorTargets[index] == nullptr)
        {
            ERROR_AND_DIE("ShadowRenderTargetManager::CreateShadowColorRT() - Failed to create shadowcolor" + std::to_string(index));
        }
    }

    // ============================================================================
    // GPU常量缓冲上传 - CreateShadowBufferIndex (Phase 3实现)
    // ============================================================================

    /**
     * @brief 根据当前FlipState生成ShadowBufferIndex并返回Bindless索引
     *
     * ShadowBufferIndex结构体:
     * ```hlsl
     * struct ShadowBufferIndex {
     *     uint shadowColorReadIndices[8];  // 读索引数组
     *     uint shadowColorWriteIndices[8]; // 写索引数组
     *     uint shadowTex0Index;            // shadowtex0索引
     *     uint shadowTex1Index;            // shadowtex1索引
     * };
     * ```
     *
     * ⚠️ 待实现: 需要D12Buffer和BindlessResourceManager集成
     * 当前返回占位符索引
     */
    uint32_t ShadowRenderTargetManager::CreateShadowBufferIndex()
    {
        // ⚠️ Phase 3实现: 生成ShadowBufferIndex并上传GPU
        // TODO: 实现GPU常量缓冲上传逻辑

        // 占位符: 返回UINT32_MAX表示未实现
        return UINT32_MAX;
    }

    // ============================================================================
    // 调试支持 - GetDebugInfo (Phase 3实现)
    // ============================================================================

    /**
     * @brief 获取指定shadowcolor的调试信息
     *
     * 教学要点:
     * - 提供详细的shadowcolor状态信息
     * - 包含创建状态、翻转状态、Bindless索引
     * - 便于调试和性能分析
     */
    std::string ShadowRenderTargetManager::GetDebugInfo(int index) const
    {
        if (!IsValidIndex(index))
        {
            return "Invalid shadowcolor index";
        }

        const auto& rt        = m_shadowColorTargets[index];
        bool        isCreated = (rt != nullptr);
        bool        isFlipped = m_flipState.IsFlipped(index);

        std::ostringstream oss;
        oss << "=== ShadowColor " << index << " (shadowcolor" << index << ") ===\n";
        oss << "Created: " << (isCreated ? "Yes" : "No (Lazy-loaded)") << "\n";

        if (isCreated)
        {
            oss << "Flip State: " << (isFlipped ? "Flipped (Read Alt, Write Main)" : "Normal (Read Main, Write Alt)") << "\n";
            oss << "Main Texture Index: " << rt->GetMainTextureIndex() << "\n";
            oss << "Alt Texture Index: " << rt->GetAltTextureIndex() << "\n";
            oss << "Settings:\n";
            oss << "  Resolution: " << m_resolution << "x" << m_resolution << " (Fixed)\n";
            oss << "  Format: " << m_settings[index].format << "\n";
            oss << "  Hardware Filtered: " << (m_settings[index].hardwareFiltered ? "Yes" : "No") << "\n";
            oss << "  Mipmap: " << (m_settings[index].enableMipmap ? "Yes" : "No") << "\n";
            oss << "  Clear Every Frame: " << (m_settings[index].clearEveryFrame ? "Yes" : "No") << "\n";
            oss << "\n" << rt->GetDebugInfo();
        }
        else
        {
            oss << "Settings (Not Created Yet):\n";
            oss << "  Resolution: " << m_resolution << "x" << m_resolution << " (Fixed)\n";
            oss << "  Format: " << m_settings[index].format << "\n";
            oss << "  Hardware Filtered: " << (m_settings[index].hardwareFiltered ? "Yes" : "No") << "\n";
            oss << "  Mipmap: " << (m_settings[index].enableMipmap ? "Yes" : "No") << "\n";
            oss << "Call GetOrCreate() to instantiate this shadowcolor RT.\n";
        }

        return oss.str();
    }

    /**
     * @brief 获取所有shadowcolor的概览信息
     *
     * 教学要点:
     * - 提供所有shadow targets的表格概览
     * - 包含8个shadowcolor + 2个shadowtex
     * - 显示创建状态、格式、Bindless索引
     */
    std::string ShadowRenderTargetManager::GetAllShadowTargetsInfo() const
    {
        std::ostringstream oss;
        oss << "=== ShadowRenderTargetManager Overview ===\n";
        oss << "Shadow Map Resolution: " << m_resolution << "x" << m_resolution << " (Fixed)\n";
        oss << "Total ShadowColor: 8 (Lazy-loaded)\n";
        oss << "Total ShadowTex: 2 (Immediate-loaded)\n\n";

        // ========================================================================
        // 表格1: ShadowColor0-7
        // ========================================================================

        oss << "--- ShadowColor Targets (shadowcolor0-7) ---\n";
        oss << "Index | Name        | Created | Flip | Main Index | Alt Index  | Format\n";
        oss << "------|-------------|---------|------|------------|------------|---------\n";

        for (int i = 0; i < 8; ++i)
        {
            const auto& rt        = m_shadowColorTargets[i];
            bool        isCreated = (rt != nullptr);
            bool        isFlipped = m_flipState.IsFlipped(i);

            char line[256];
            if (isCreated)
            {
                sprintf_s(line, "%-5d | shadowcolor%-1d | %-7s | %-4s | %-10u | %-10u | %-6d\n",
                          i,
                          i,
                          "Yes",
                          isFlipped ? "Yes" : "No",
                          rt->GetMainTextureIndex(),
                          rt->GetAltTextureIndex(),
                          m_settings[i].format
                );
            }
            else
            {
                sprintf_s(line, "%-5d | shadowcolor%-1d | %-7s | %-4s | %-10s | %-10s | %-6d\n",
                          i,
                          i,
                          "No",
                          "N/A",
                          "N/A",
                          "N/A",
                          m_settings[i].format
                );
            }
            oss << line;
        }

        oss << "\n";

        // ========================================================================
        // 表格2: ShadowTex0/1
        // ========================================================================

        oss << "--- ShadowTex Depth Targets (shadowtex0-1) ---\n";
        oss << "Name        | Created | Bindless Index | Type\n";
        oss << "------------|---------|----------------|----------\n";

        // shadowtex0
        {
            bool isCreated = (m_shadowTex0 != nullptr);
            char line[256];
            if (isCreated)
            {
                sprintf_s(line, "shadowtex0  | %-7s | %-14u | ShadowMap\n",
                          "Yes",
                          m_shadowTex0->GetBindlessIndex()
                );
            }
            else
            {
                sprintf_s(line, "shadowtex0  | %-7s | %-14s | ShadowMap\n",
                          "No",
                          "N/A"
                );
            }
            oss << line;
        }

        // shadowtex1
        {
            bool isCreated = (m_shadowTex1 != nullptr);
            char line[256];
            if (isCreated)
            {
                sprintf_s(line, "shadowtex1  | %-7s | %-14u | ShadowMap\n",
                          "Yes",
                          m_shadowTex1->GetBindlessIndex()
                );
            }
            else
            {
                sprintf_s(line, "shadowtex1  | %-7s | %-14s | ShadowMap\n",
                          "No",
                          "N/A"
                );
            }
            oss << line;
        }

        return oss.str();
    }
} // namespace enigma::graphic
