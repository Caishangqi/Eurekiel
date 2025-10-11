#pragma once

#include <memory>
#include <string>
#include <optional>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>

namespace enigma::graphic
{
    class BindlessIndexAllocator;
    /**
     * @brief D12Resource类 - DirectX 12资源的基础封装
     *
     * 教学要点:
     * 1. 这是所有DX12资源的基类，封装ID3D12Resource的公共操作
     * 2. 使用RAII管理资源生命周期，确保正确释放GPU内存
     * 3. 支持资源状态追踪，这是DX12的重要特性
     * 4. 专门为Bindless架构和延迟渲染设计
     *
     * DirectX 12特性:
     * - 显式资源状态管理 (不同于DX11的隐式状态)
     * - 支持资源别名和子资源
     * - GPU虚拟地址访问 (用于Bindless绑定)
     *
     * 架构设计 (Milestone 2.3):
     * - **可选Bindless支持**: 使用ResourceBindingTraits组合模式
     * - **零开销抽象**: 不使用Bindless时无额外性能开销
     * - **职责分离**: 资源管理与绑定管理分离
     * - **类型安全**: 编译时确定资源是否支持Bindless
     * - **统一注册接口**: 所有资源类型使用相同的便捷注册方法
     *
     * @note 这是新渲染系统专用的资源封装，支持现代Bindless架构
     */
    class D12Resource : public std::enable_shared_from_this<D12Resource>
    {
    protected:
        ID3D12Resource*       m_resource; // DX12资源指针
        D3D12_RESOURCE_STATES m_currentState; // 当前资源状态
        std::string           m_debugName; // 调试名称
        size_t                m_size; // 资源大小 (字节)
        bool                  m_isValid; // 资源有效性标记

        // SM6.6 Bindless索引 (Milestone 2.7) - 替代ResourceBindingTraits
        static constexpr uint32_t INVALID_BINDLESS_INDEX = UINT32_MAX;
        uint32_t                  m_bindlessIndex        = INVALID_BINDLESS_INDEX; // Bindless全局索引

        // CPU数据管理 (Milestone 2.7) - True Bindless上传支持
        std::vector<uint8_t> m_cpuData; // CPU端数据缓存
        bool                 m_isUploaded = false; // 上传状态标记 - 用于Bindless注册安全检查

    public:
        /**
         * @brief 构造函数
         * 
         * 教学要点: 基类构造函数初始化通用属性
         */
        D12Resource();

        /**
         * @brief 虚析构函数
         * 
         * 教学要点: 虚析构函数确保派生类正确释放资源
         */
        virtual ~D12Resource();

        // ========================================================================
        // 资源访问接口
        // ========================================================================

        /**
         * @brief 获取DX12资源指针
         * @return ID3D12Resource指针
         */
        ID3D12Resource* GetResource() const { return m_resource; }

        /**
         * @brief 获取GPU虚拟地址 (用于Bindless绑定)
         * @return GPU虚拟地址
         * 
         * 教学要点: GPU虚拟地址是DX12 Bindless资源绑定的关键
         */
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;

        /**
         * @brief 获取当前资源状态
         * @return 资源状态枚举
         */
        D3D12_RESOURCE_STATES GetCurrentState() const { return m_currentState; }

        /**
         * @brief 设置资源状态 (通常由资源状态追踪器调用)
         * @param newState 新的资源状态
         */
        void SetCurrentState(D3D12_RESOURCE_STATES newState) { m_currentState = newState; }

        /**
         * @brief 获取资源大小
         * @return 资源大小 (字节)
         */
        size_t GetSize() const { return m_size; }

        /**
         * @brief 检查资源是否有效
         * @return 有效返回true，否则返回false
         */
        bool IsValid() const { return m_isValid && m_resource != nullptr; }

        /**
         * @brief 设置调试名称 (用于PIX调试) - 虚函数支持子类自定义行为
         * @param name 调试名称
         *
         * 教学要点: 虚函数允许子类重写调试名称设置逻辑
         * 例如，纹理类可以在设置名称时同时更新纹理视图的名称
         */
        virtual void SetDebugName(const std::string& name);

        /**
         * @brief 获取调试名称 - 虚函数支持子类自定义格式
         * @return 调试名称字符串
         *
         * 教学要点: 虚函数允许子类返回格式化的调试信息
         * 例如，缓冲区类可以在名称中包含大小信息
         */
        virtual const std::string& GetDebugName() const { return m_debugName; }

        /**
         * @brief 获取详细调试信息 - 纯虚函数要求子类必须实现
         * @return 详细调试信息字符串
         *
         * 教学要点: 纯虚函数强制所有子类提供具体的调试信息实现
         * 这遵循了接口隔离原则，每个资源类型都有其特定的调试信息
         */
        virtual std::string GetDebugInfo() const = 0;

        // ========================================================================
        // CPU数据管理接口 (Milestone 2.7) - True Bindless上传支持
        // ========================================================================

        /**
         * @brief 设置初始数据(CPU端)
         * @param data 数据指针
         * @param dataSize 数据大小(字节)
         *
         * 教学要点:
         * 1. True Bindless流程: Create → SetInitialData → Upload → RegisterBindless
         * 2. 数据复制到m_cpuData，等待Upload()调用
         * 3. 上传完成后可以选择保留或释放CPU数据
         */
        void SetInitialData(const void* data, size_t dataSize);

        /**
         * @brief 检查是否有CPU数据
         * @return 有数据返回true
         */
        bool HasCPUData() const { return !m_cpuData.empty(); }

        /**
         * @brief 获取CPU数据指针
         * @return CPU数据指针，无数据返回nullptr
         */
        const void* GetCPUData() const { return m_cpuData.empty() ? nullptr : m_cpuData.data(); }

        /**
         * @brief 获取CPU数据大小
         * @return 数据大小(字节)
         */
        size_t GetCPUDataSize() const { return m_cpuData.size(); }

        /**
         * @brief 释放CPU数据(上传完成后)
         *
         * 教学要点: 上传完成后释放CPU内存，节省内存占用
         */
        void ReleaseCPUData()
        {
            m_cpuData.clear();
            m_cpuData.shrink_to_fit();
        }

        /**
         * @brief 上传资源到GPU
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. Template Method模式: 基类管理流程，子类实现细节
         * 2. 使用Copy Command List专用队列
         * 3. 同步等待上传完成(KISS原则)
         * 4. 上传后设置m_isUploaded = true
         */
        bool Upload();

        /**
         * @brief 检查资源是否已上传到GPU
         * @return 已上传返回true，否则返回false
         *
         * 教学要点:
         * 1. 安全检查: 用于Bindless注册前验证资源状态
         * 2. True Bindless流程: Create → Upload → Register
         * 3. 防止运行时错误: GPU访问未上传资源会导致黑屏/崩溃
         */
        bool IsUploaded() const
        {
            return m_isUploaded;
        }

        // ========================================================================
        // SM6.6 Bindless资源管理接口 (Milestone 2.7简化版)
        // ========================================================================

        /**
         * @brief 检查是否已注册到Bindless系统
         * @return 已注册返回true(索引有效)
         *
         * 教学要点:
         * 1. SM6.6简化设计：从两层检查简化为单一索引有效性检查
         * 2. 零额外开销：不再使用std::optional,直接判断索引
         */
        bool IsBindlessRegistered() const
        {
            return m_bindlessIndex != INVALID_BINDLESS_INDEX;
        }

        /**
         * @brief 获取Bindless全局索引
         * @return 全局索引,INVALID_BINDLESS_INDEX表示未注册
         *
         * 教学要点:
         * 1. SM6.6简化：从std::optional<uint32_t>改为uint32_t直接返回
         * 2. 使用INVALID_BINDLESS_INDEX代替std::nullopt
         * 3. 更高效,无额外堆内存分配
         */
        uint32_t GetBindlessIndex() const { return m_bindlessIndex; }

        /**
         * @brief 设置Bindless索引(由BindlessIndexAllocator调用)
         * @param index 全局索引
         *
         * 教学要点:
         * 1. SM6.6简化：移除DescriptorHandle参数
         * 2. 直接设置索引,无需检查启用状态
         * 3. 描述符创建由CreateDescriptorInGlobalHeap()负责
         */
        void SetBindlessIndex(uint32_t index)
        {
            m_bindlessIndex = index;
        }

        /**
         * @brief 清除Bindless索引
         *
         * 教学要点: SM6.6简化 - 直接重置索引为无效值
         */
        void ClearBindlessIndex()
        {
            m_bindlessIndex = INVALID_BINDLESS_INDEX;
        }

        // ========================================================================
        // GPU资源上传接口 (Milestone 2.7) - 纯虚函数由子类实现
        // ========================================================================

        /**
         * @brief 上传资源到GPU (纯虚函数 - 由子类实现具体上传逻辑)
         * @param commandList Copy Command List
         * @param uploadContext Upload Heap上下文
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. Template Method模式: 基类Upload()管理流程，子类UploadToGPU()实现细节
         * 2. 子类实现: D12Texture使用UploadTextureData(), D12Buffer使用UploadBufferData()
         * 3. Copy Command List: 专用队列，避免阻塞Graphics/Compute队列
         */
        virtual bool UploadToGPU(
            ID3D12GraphicsCommandList* commandList,
            class UploadContext&       uploadContext
        ) = 0;

        /**
         * @brief 获取上传后的目标资源状态 (纯虚函数 - 由子类指定)
         * @return 上传完成后资源应处于的状态
         *
         * 教学要点:
         * 1. D12Texture返回: D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE (像素着色器资源)
         * 2. D12Buffer返回: D3D12_RESOURCE_STATE_GENERIC_READ (通用读取)
         * 3. 状态转换由基类Upload()统一管理
         */
        virtual D3D12_RESOURCE_STATES GetUploadDestinationState() const = 0;

        /**
         * @brief 在全局描述符堆中创建描述符(纯虚函数)
         * @param device DX12设备
         * @param heapManager 全局描述符堆管理器
         *
         * 教学要点:
         * 1. SM6.6架构中,描述符创建与索引分配分离
         * 2. 子类必须实现如何在全局堆中创建自己的描述符
         * 3. 描述符创建到指定索引(m_bindlessIndex)
         * 4. 全局堆容量1M,支持百万级资源
         *
         * 实现示例(D12Texture):
         * ```cpp
         * void CreateDescriptorInGlobalHeap(ID3D12Device* device,
         *                                   GlobalDescriptorHeapManager* heapManager) override
         * {
         *     if (!IsBindlessRegistered())
         *         return;
         *
         *     D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
         *     // ... 配置srvDesc ...
         *     heapManager->CreateShaderResourceView(device, m_resource, &srvDesc, m_bindlessIndex);
         * }
         * ```
         */
        virtual void CreateDescriptorInGlobalHeap(
            ID3D12Device*                      device,
            class GlobalDescriptorHeapManager* heapManager) = 0;

        // ========================================================================
        // Bindless资源注册接口 (Milestone 2.7) - SM6.6简化架构
        // ========================================================================

        /**
         * @brief 注册到Bindless系统
         * @return 成功返回全局索引，失败返回nullopt
         *
         * 教学要点:
         * 1. SM6.6 Bindless架构: BindlessIndexAllocator(索引) + GlobalDescriptorHeapManager(描述符)
         * 2. True Bindless流程: Create() → Upload() → RegisterBindless()
         * 3. 安全检查: 必须先Upload()才能RegisterBindless()
         * 4. 三步注册: 分配索引 → 存储索引 → 创建描述符
         *
         * 使用示例:
         * ```cpp
         * auto texture = D12Texture::Create(...);
         * texture->SetInitialData(...);    // 设置CPU数据
         * texture->Upload();                // 上传到GPU
         * auto index = texture->RegisterBindless();  // 注册到Bindless系统
         * // 现在可以在Shader中通过index访问: ResourceDescriptorHeap[index]
         * ```
         */
        std::optional<uint32_t> RegisterBindless();

        /**
         * @brief 从Bindless系统注销
         * @return 成功返回true，失败返回false
         *
         * 教学要点:
         * 1. RAII设计: 确保资源正确注销，避免索引泄漏
         * 2. 架构分离: 只释放索引，描述符堆自动管理
         * 3. 幂等操作: 未注册时调用也是安全的
         *
         * 使用示例:
         * ```cpp
         * texture->UnregisterBindless();  // 释放Bindless索引
         * // 资源仍然有效，但不再通过Bindless访问
         * ```
         */
        bool UnregisterBindless();

    protected:
        /**
         * @brief 分配Bindless索引（纯虚函数 - 子类决定纹理或缓冲区）
         * @param allocator 索引分配器
         * @return 成功返回索引，失败返回INVALID_BINDLESS_INDEX
         *
         * 教学要点:
         * 1. Template Method模式 - 基类管理流程，子类决定索引类型
         * 2. D12Texture返回: allocator->AllocateTextureIndex()
         * 3. D12Buffer返回: allocator->AllocateBufferIndex()
         */
        virtual uint32_t AllocateBindlessIndexInternal(
            BindlessIndexAllocator* allocator) const = 0;

        /**
         * @brief 释放Bindless索引（纯虚函数）
         * @param allocator 索引分配器
         * @param index 要释放的索引
         * @return 成功返回true
         *
         * 教学要点:
         * 1. Template Method模式 - 子类实现具体释放逻辑
         * 2. D12Texture调用: allocator->FreeTextureIndex(index)
         * 3. D12Buffer调用: allocator->FreeBufferIndex(index)
         */
        virtual bool FreeBindlessIndexInternal(
            BindlessIndexAllocator* allocator, uint32_t index) const = 0;

    protected:
        /**
         * @brief 设置资源指针 (由派生类调用)
         * @param resource DX12资源指针
         * @param initialState 初始资源状态
         * @param size 资源大小
         */
        void SetResource(ID3D12Resource* resource, D3D12_RESOURCE_STATES initialState, size_t size);

        /**
         * @brief 释放资源
         * 
         * 教学要点: 安全释放DX12资源，避免重复释放
         */
        virtual void ReleaseResource();

    protected:
        // 支持移动语义（派生类需要访问）
        D12Resource(D12Resource&&)            = default;
        D12Resource& operator=(D12Resource&&) = default;

    private:
        // 禁用拷贝构造和赋值
        D12Resource(const D12Resource&)            = delete;
        D12Resource& operator=(const D12Resource&) = delete;
    };
} // namespace enigma::graphic
