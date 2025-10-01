#include "UniformBuffer.hpp"
#include "Engine/Core/Logger/LoggerAPI.hpp"
#include "Engine/Graphic/Integration/RendererSubsystem.hpp"
#include "Engine/Graphic/Resource/Buffer/D12Buffer.hpp"
#include "Engine/Graphic/Core/DX12/D3D12RenderSystem.hpp"
#include <cstring>

namespace enigma::graphic
{
    /**
     * @brief 构造函数 - 初始化为零状态
     *
     * 教学要点:
     * 1. RAII原则: 构造时不分配GPU资源,通过Initialize()延迟初始化
     * 2. True Bindless流程: Construct → Initialize → Update → Upload
     * 3. 零初始化: 所有数据清零,避免未定义行为
     */
    UniformBuffer::UniformBuffer()
        : m_gpuUniformBuffer(nullptr)
          , m_renderTargetInfoBuffer(nullptr)
          , m_isInitialized(false)
          , m_isDirty(true) // 初始状态为脏,强制首次上传
          , m_renderTargetInfoDirty(true)
    {
        // 零初始化Root Constants
        memset(&m_rootConstants, 0, sizeof(RootConstants));

        // 零初始化IrisUniformBuffer
        memset(&m_gpuUniformData, 0, sizeof(IrisUniformBuffer));

        // 零初始化RenderTargetInfo
        memset(&m_renderTargetInfo, 0, sizeof(RenderTargetInfo));

        // 设置无效索引 (INVALID_BINDLESS_INDEX = UINT32_MAX)
        for (int i = 0; i < 16; ++i)
        {
            m_renderTargetInfo.colorTexMainIndices[i] = UINT32_MAX;
            m_renderTargetInfo.colorTexAltIndices[i]  = UINT32_MAX;
        }

        core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                      "UniformBuffer: Constructed (buffers not yet created)");
    }

    /**
     * @brief 析构函数 - RAII资源清理
     *
     * 教学要点:
     * 1. 智能指针自动清理: D12Buffer内部使用ComPtr管理DirectX资源
     * 2. 注销Bindless索引: 确保索引不泄漏
     */
    UniformBuffer::~UniformBuffer()
    {
        if (m_gpuUniformBuffer)
        {
            // 注销Bindless索引 (如果已注册)
            if (m_gpuUniformBuffer->IsBindlessRegistered())
            {
                m_gpuUniformBuffer->UnregisterBindless();
            }
            delete m_gpuUniformBuffer;
            m_gpuUniformBuffer = nullptr;
        }

        if (m_renderTargetInfoBuffer)
        {
            if (m_renderTargetInfoBuffer->IsBindlessRegistered())
            {
                m_renderTargetInfoBuffer->UnregisterBindless();
            }
            delete m_renderTargetInfoBuffer;
            m_renderTargetInfoBuffer = nullptr;
        }

        core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                      "UniformBuffer: Destroyed");
    }

    /**
     * @brief 初始化GPU Buffers并注册到Bindless系统
     *
     * 教学要点:
     * 1. True Bindless流程: Create → SetInitialData → Upload → RegisterBindless
     * 2. 两个GPU Buffer:
     *    - IrisUniformBuffer: 存储98个Iris uniforms (~1352 bytes对齐到256)
     *    - RenderTargetInfo: 存储32个纹理索引 (128 bytes)
     * 3. D3D12_RESOURCE_STATE_GENERIC_READ: StructuredBuffer的标准状态
     * 4. 256字节对齐: D3D12 Constant Buffer的硬件要求
     */
    bool UniformBuffer::Initialize()
    {
        if (m_isInitialized)
        {
            core::LogWarn(RendererSubsystem::GetStaticSubsystemName(),
                          "UniformBuffer: Already initialized");
            return true;
        }

        auto* device = D3D12RenderSystem::GetDevice();
        if (!device)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UniformBuffer: Failed to get D3D12 device");
            return false;
        }

        // ========================================================================
        // 1. 创建IrisUniformBuffer (GPU Bindless Buffer)
        // ========================================================================

        // 计算对齐后的大小 (256字节对齐)
        constexpr size_t ALIGNMENT           = 256;
        const size_t     irisUniformDataSize = sizeof(IrisUniformBuffer);
        const size_t     alignedUniformSize  = (irisUniformDataSize + ALIGNMENT - 1) & ~(ALIGNMENT - 1);

        // 创建GPU Buffer (使用BufferCreateInfo)
        BufferCreateInfo uniformBufferInfo;
        uniformBufferInfo.size         = alignedUniformSize;
        uniformBufferInfo.usage        = BufferUsage::StructuredBuffer;
        uniformBufferInfo.memoryAccess = MemoryAccess::GPUOnly;
        uniformBufferInfo.initialData  = nullptr; // 稍后通过SetInitialData设置
        uniformBufferInfo.debugName    = "IrisUniformBuffer";

        m_gpuUniformBuffer = new D12Buffer(uniformBufferInfo);

        if (!m_gpuUniformBuffer || !m_gpuUniformBuffer->IsValid())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UniformBuffer: Failed to create IrisUniformBuffer");
            if (m_gpuUniformBuffer)
            {
                delete m_gpuUniformBuffer;
                m_gpuUniformBuffer = nullptr;
            }
            return false;
        }

        // 设置初始数据 (CPU端缓存)
        m_gpuUniformBuffer->SetInitialData(&m_gpuUniformData, irisUniformDataSize);

        // 上传到GPU
        if (!m_gpuUniformBuffer->Upload())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UniformBuffer: Failed to upload IrisUniformBuffer");
            delete m_gpuUniformBuffer;
            m_gpuUniformBuffer = nullptr;
            return false;
        }

        // 注册到Bindless系统
        auto uniformIndex = m_gpuUniformBuffer->RegisterBindless();
        if (!uniformIndex.has_value())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UniformBuffer: Failed to register IrisUniformBuffer to Bindless");
            delete m_gpuUniformBuffer;
            m_gpuUniformBuffer = nullptr;
            return false;
        }

        // 存储索引到Root Constants
        m_rootConstants.irisUniformBufferIndex = uniformIndex.value();

        core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                      "UniformBuffer: IrisUniformBuffer created (size=%zu bytes aligned to %zu, index=%u)",
                      irisUniformDataSize, alignedUniformSize, uniformIndex.value());

        // ========================================================================
        // 2. 创建RenderTargetInfo Buffer (GPU Bindless Buffer)
        // ========================================================================

        const size_t renderTargetInfoSize = sizeof(RenderTargetInfo);

        // 创建GPU Buffer (128字节已对齐,无需额外对齐)
        BufferCreateInfo renderTargetBufferInfo;
        renderTargetBufferInfo.size         = renderTargetInfoSize;
        renderTargetBufferInfo.usage        = BufferUsage::StructuredBuffer;
        renderTargetBufferInfo.memoryAccess = MemoryAccess::GPUOnly;
        renderTargetBufferInfo.initialData  = nullptr;
        renderTargetBufferInfo.debugName    = "RenderTargetInfo";

        m_renderTargetInfoBuffer = new D12Buffer(renderTargetBufferInfo);

        if (!m_renderTargetInfoBuffer || !m_renderTargetInfoBuffer->IsValid())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UniformBuffer: Failed to create RenderTargetInfo buffer");
            delete m_gpuUniformBuffer;
            m_gpuUniformBuffer = nullptr;
            if (m_renderTargetInfoBuffer)
            {
                delete m_renderTargetInfoBuffer;
                m_renderTargetInfoBuffer = nullptr;
            }
            return false;
        }

        // 设置初始数据
        m_renderTargetInfoBuffer->SetInitialData(&m_renderTargetInfo, renderTargetInfoSize);

        // 上传到GPU
        if (!m_renderTargetInfoBuffer->Upload())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UniformBuffer: Failed to upload RenderTargetInfo buffer");
            delete m_gpuUniformBuffer;
            m_gpuUniformBuffer = nullptr;
            delete m_renderTargetInfoBuffer;
            m_renderTargetInfoBuffer = nullptr;
            return false;
        }

        // 注册到Bindless系统
        auto renderTargetIndex = m_renderTargetInfoBuffer->RegisterBindless();
        if (!renderTargetIndex.has_value())
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UniformBuffer: Failed to register RenderTargetInfo to Bindless");
            delete m_gpuUniformBuffer;
            m_gpuUniformBuffer = nullptr;
            delete m_renderTargetInfoBuffer;
            m_renderTargetInfoBuffer = nullptr;
            return false;
        }

        // 存储索引到Root Constants
        m_rootConstants.renderTargetInfoIndex = renderTargetIndex.value();

        core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                      "UniformBuffer: RenderTargetInfo created (size=%zu bytes, index=%u)",
                      renderTargetInfoSize, renderTargetIndex.value());

        // ========================================================================
        // 3. 初始化完成
        // ========================================================================

        m_isInitialized = true;

        core::LogInfo(RendererSubsystem::GetStaticSubsystemName(),
                      "UniformBuffer: Initialization complete (IrisUniform=%u, RenderTargetInfo=%u)",
                      m_rootConstants.irisUniformBufferIndex,
                      m_rootConstants.renderTargetInfoIndex);

        return true;
    }

    /**
     * @brief 更新所有脏数据到GPU
     *
     * 教学要点:
     * 1. 延迟上传: 只在UpdateAll()时才同步到GPU,减少GPU拷贝次数
     * 2. 脏标记优化: 只上传修改过的Buffer
     * 3. Upload Heap自动管理: UploadContext内部使用Copy Queue专用队列
     */
    bool UniformBuffer::UpdateAll()
    {
        if (!m_isInitialized)
        {
            core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                           "UniformBuffer: Cannot update before initialization");
            return false;
        }

        bool uploadSuccess = true;

        // ========================================================================
        // 1. 更新IrisUniformBuffer (如果脏)
        // ========================================================================

        if (m_isDirty && m_gpuUniformBuffer)
        {
            // 更新CPU数据缓存
            m_gpuUniformBuffer->SetInitialData(&m_gpuUniformData, sizeof(IrisUniformBuffer));

            // 上传到GPU
            if (!m_gpuUniformBuffer->Upload())
            {
                core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "UniformBuffer: Failed to upload IrisUniformBuffer");
                uploadSuccess = false;
            }
            else
            {
                m_isDirty = false; // 清除脏标记
            }
        }

        // ========================================================================
        // 2. 更新RenderTargetInfo (如果脏)
        // ========================================================================

        if (m_renderTargetInfoDirty && m_renderTargetInfoBuffer)
        {
            // 更新CPU数据缓存
            m_renderTargetInfoBuffer->SetInitialData(&m_renderTargetInfo, sizeof(RenderTargetInfo));

            // 上传到GPU
            if (!m_renderTargetInfoBuffer->Upload())
            {
                core::LogError(RendererSubsystem::GetStaticSubsystemName(),
                               "UniformBuffer: Failed to upload RenderTargetInfo");
                uploadSuccess = false;
            }
            else
            {
                m_renderTargetInfoDirty = false; // 清除脏标记
            }
        }

        return uploadSuccess;
    }

    // ========================================================================
    // RenderTarget信息更新接口
    // ========================================================================

    /**
     * @brief 更新RenderTarget信息 (colortex0-15 Main/Alt纹理)
     *
     * 教学要点:
     * 1. Ping-Pong渲染: Main和Alt交替作为读/写目标
     * 2. Iris架构: stageWritesToMain决定写Main还是Alt (RenderTargets.java:355)
     * 3. 延迟上传: 只标记脏,UpdateAll()时统一上传
     */
    void UniformBuffer::UpdateRenderTargetInfo(
        const uint32_t colorTexMainIndices[16],
        const uint32_t colorTexAltIndices[16]
    )
    {
        // 复制Main纹理索引
        memcpy(m_renderTargetInfo.colorTexMainIndices, colorTexMainIndices, sizeof(uint32_t) * 16);

        // 复制Alt纹理索引
        memcpy(m_renderTargetInfo.colorTexAltIndices, colorTexAltIndices, sizeof(uint32_t) * 16);

        // 标记为脏 (需要上传到GPU)
        m_renderTargetInfoDirty = true;
    }

    /**
     * @brief 更新深度/阴影/噪声纹理索引 (Root Constants直接存储)
     *
     * 教学要点:
     * 1. 这些纹理索引直接存储在Root Constants,无需额外GPU Buffer
     * 2. Root Constants特性: 每次SetGraphicsRoot32BitConstants创建独立副本
     * 3. 深度纹理: depthtex0 (主), depthtex1 (半透明前副本), depthtex2 (无手部副本)
     * 4. 阴影纹理: shadowtex0 (深度), shadowtex1 (半透明深度)
     */
    void UniformBuffer::UpdateDepthShadowNoiseIndices(
        const uint32_t depthTexIndices[3],
        const uint32_t shadowTexIndices[2],
        uint32_t       noisetexIndex
    )
    {
        // 更新深度纹理索引
        m_rootConstants.depthTex0Index = depthTexIndices[0];
        m_rootConstants.depthTex1Index = depthTexIndices[1];
        m_rootConstants.depthTex2Index = depthTexIndices[2];

        // 更新阴影纹理索引
        m_rootConstants.shadowTex0Index = shadowTexIndices[0];
        m_rootConstants.shadowTex1Index = shadowTexIndices[1];

        // 更新噪声纹理索引
        m_rootConstants.noisetexIndex = noisetexIndex;

        // 注意: Root Constants不需要脏标记,每次绘制都会重新设置
    }

    // ========================================================================
    // 时间相关Uniform更新
    // ========================================================================

    /**
     * @brief 更新帧时间相关Uniforms
     *
     * 教学要点:
     * 1. 高频更新: 这些数据每帧都会变化,放在Root Constants减少GPU访问
     * 2. Iris对应: frameTimeCounter, frameCounter
     */
    void UniformBuffer::UpdateFrameTime(float frameTime, uint32_t frameCounter)
    {
        // 更新Root Constants (高频数据)
        m_rootConstants.frameTime    = frameTime;
        m_rootConstants.frameCounter = frameCounter;

        // 更新IrisUniformBuffer (GPU端完整数据)
        m_gpuUniformData.frameTime    = frameTime;
        m_gpuUniformData.frameCounter = static_cast<int>(frameCounter);

        m_isDirty = true;
    }

    /**
     * @brief 更新世界时间相关Uniforms
     *
     * 教学要点:
     * 1. Iris worldTime: Minecraft世界内部时间 (ticks)
     * 2. Iris worldDay: 世界天数
     * 3. 时间计算: moonPhase = worldDay % 8 (月相周期8天)
     */
    void UniformBuffer::UpdateWorldTime(int worldTime, int worldDay)
    {
        m_gpuUniformData.worldTime = worldTime;
        m_gpuUniformData.worldDay  = worldDay;

        // 自动计算月相 (8天周期)
        m_gpuUniformData.moonPhase = worldDay % 8;

        m_isDirty = true;
    }

    /**
     * @brief 更新太阳和月亮角度
     *
     * 教学要点:
     * 1. Iris架构: sunAngle和shadowAngle用于计算日夜光照
     * 2. moonPhase: 月相 (0-7, 对应Minecraft的8个月相)
     */
    void UniformBuffer::UpdateCelestialAngles(float sunAngle, float shadowAngle, int32_t moonPhase)
    {
        m_gpuUniformData.sunAngle    = sunAngle;
        m_gpuUniformData.shadowAngle = shadowAngle;
        m_gpuUniformData.moonPhase   = moonPhase;

        m_isDirty = true;
    }

    // ========================================================================
    // 相机和视图矩阵更新
    // ========================================================================

    /**
     * @brief 更新相机位置
     */
    void UniformBuffer::UpdateCameraPosition(const Vec3& position)
    {
        m_gpuUniformData.cameraPosition = position;
        m_isDirty                       = true;
    }

    /**
     * @brief 更新视图矩阵 (modelView, projection, MVP)
     *
     * 教学要点:
     * 1. Iris架构: gbufferModelView, gbufferProjection 分别存储
     * 2. 自动计算MVP: gbufferModelViewProjection = gbufferProjection * gbufferModelView
     * 3. 坐标系: +x forward, +y left, +z up (Enigma引擎约定)
     */
    void UniformBuffer::UpdateViewMatrices(const Mat44& modelView, const Mat44& projection)
    {
        m_gpuUniformData.gbufferModelView         = modelView;
        m_gpuUniformData.gbufferProjection        = projection;
        m_gpuUniformData.gbufferModelViewInverse  = modelView.GetOrthonormalInverse();
        m_gpuUniformData.gbufferProjectionInverse = projection.GetOrthonormalInverse();

        // 自动计算MVP矩阵 (使用Mat44::Append - 右乘/列向量表示法)
        // MVP = Projection * ModelView (列向量: P * MV * v)
        m_gpuUniformData.mvpMatrix = projection;
        m_gpuUniformData.mvpMatrix.Append(modelView);

        m_isDirty = true;
    }

    // ========================================================================
    // MVP和视图矩阵更新 (声明在头文件中的方法)
    // ========================================================================

    /**
     * @brief 更新MVP矩阵
     */
    void UniformBuffer::UpdateMVPMatrix(const Mat44& mvp)
    {
        m_gpuUniformData.mvpMatrix = mvp;
        m_isDirty                  = true;
    }

    /**
     * @brief 更新模型视图矩阵
     */
    void UniformBuffer::UpdateModelViewMatrix(const Mat44& modelView)
    {
        m_gpuUniformData.modelViewMatrix = modelView;
        m_isDirty                        = true;
    }

    /**
     * @brief 更新投影矩阵
     */
    void UniformBuffer::UpdateProjectionMatrix(const Mat44& projection)
    {
        m_gpuUniformData.projectionMatrix = projection;
        m_isDirty                         = true;
    }

    /**
     * @brief 更新上一帧相机位置
     */
    void UniformBuffer::UpdatePreviousCameraPosition(const Vec3& previousPosition)
    {
        m_gpuUniformData.previousCameraPosition = previousPosition;
        m_isDirty                               = true;
    }

    /**
     * @brief 更新眼睛位置
     */
    void UniformBuffer::UpdateEyePosition(const Vec3& eyePosition)
    {
        m_gpuUniformData.eyePosition = eyePosition;
        m_isDirty                    = true;
    }

    /**
     * @brief 更新太阳/阴影角度
     */
    void UniformBuffer::UpdateSunAngles(float sunAngle, float shadowAngle)
    {
        m_gpuUniformData.sunAngle    = sunAngle;
        m_gpuUniformData.shadowAngle = shadowAngle;
        m_isDirty                    = true;
    }

    /**
     * @brief 重置为默认值
     */
    void UniformBuffer::Reset()
    {
        // 重置Root Constants
        memset(&m_rootConstants, 0, sizeof(RootConstants));

        // 重置IrisUniformBuffer
        memset(&m_gpuUniformData, 0, sizeof(IrisUniformBuffer));

        // 重置RenderTargetInfo
        memset(&m_renderTargetInfo, 0, sizeof(RenderTargetInfo));

        // 设置无效索引
        for (int i = 0; i < 16; ++i)
        {
            m_renderTargetInfo.colorTexMainIndices[i] = UINT32_MAX;
            m_renderTargetInfo.colorTexAltIndices[i]  = UINT32_MAX;
        }

        // 标记为脏
        m_isDirty               = true;
        m_renderTargetInfoDirty = true;
    }

    // ========================================================================
    // 实体和方块ID更新 (高频)
    // ========================================================================

    /**
     * @brief 更新当前绘制实体ID (每个实体绘制前调用)
     *
     * 教学要点:
     * 1. 高频更新: 每个实体绘制前都会变化
     * 2. 存储在Root Constants: 避免GPU Buffer访问开销
     */
    void UniformBuffer::UpdateEntityID(uint32_t entityId, uint32_t blockId)
    {
        m_rootConstants.entityId = entityId;
        m_rootConstants.blockId  = blockId;
        // Root Constants不需要脏标记,每次绘制都会重新设置
    }

    // ========================================================================
    // Getter方法
    // ========================================================================

    /**
     * @brief 获取IrisUniformBuffer的Bindless索引
     */
    uint32_t UniformBuffer::GetIrisUniformBufferIndex() const
    {
        return m_rootConstants.irisUniformBufferIndex;
    }

    /**
     * @brief 获取RenderTargetInfo的Bindless索引
     */
    uint32_t UniformBuffer::GetRenderTargetInfoIndex() const
    {
        return m_rootConstants.renderTargetInfoIndex;
    }

    /**
     * @brief 检查是否已初始化
     */
    bool UniformBuffer::IsInitialized() const
    {
        return m_isInitialized;
    }
} // namespace enigma::graphic
