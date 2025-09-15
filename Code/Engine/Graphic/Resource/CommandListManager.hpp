#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <mutex>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace enigma::graphic
{
    /**
     * @brief CommandListManager类 - DirectX 12命令列表管理器
     * 
     * 教学要点:
     * 1. DirectX 12的核心概念：显式命令列表管理 vs DX11的隐式立即上下文
     * 2. 命令列表池化技术：复用命令列表避免频繁创建销毁的开销
     * 3. 多线程并行录制：不同线程可以同时录制不同的命令列表
     * 4. GPU/CPU同步：使用围栏(Fence)确保GPU完成执行后再复用命令列表
     * 
     * DirectX 12特性:
     * - 支持三种命令队列类型：Graphics、Compute、Copy
     * - 命令分配器(CommandAllocator)管理底层内存
     * - 围栏同步机制避免CPU/GPU竞态条件
     * - 资源状态转换需要显式管理
     * 
     * 延迟渲染应用:
     * - 多个渲染阶段可以并行录制命令
     * - G-Buffer写入和读取的状态转换管理
     * - Compute Shader和Graphics Pipeline的协调
     * 
     * @note 这是为Enigma渲染器专门设计的命令管理系统
     */
    class CommandListManager final
    {
    public:
        /**
         * @brief 命令列表类型枚举
         * 
         * 教学要点: DX12支持三种不同用途的命令队列
         */
        enum class Type
        {
            Graphics, // 图形命令 (绘制、状态设置等)
            Compute, // 计算命令 (Compute Shader调度)
            Copy // 复制命令 (资源复制、上传等)
        };

        /**
         * @brief 命令列表状态枚举
         */
        enum class State
        {
            Ready, // 就绪状态，可以开始录制
            Recording, // 正在录制中
            Closed, // 已关闭，可以提交执行
            Executing, // GPU正在执行中
            Completed // GPU执行完成，可以重置复用
        };

    private:
        /**
         * @brief 命令列表包装器
         * 
         * 教学要点: 将DX12的原生对象包装为更易用的C++对象，使用智能指针确保RAII
         */
        struct CommandListWrapper
        {
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList; // 命令列表 (智能指针)
            Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    commandAllocator; // 命令分配器 (智能指针)
            State                                             state; // 当前状态
            uint64_t                                          fenceValue; // 关联的围栏值
            Type                                              type; // 命令列表类型
            std::string                                       debugName; // 调试名称

            CommandListWrapper();
            ~CommandListWrapper();

            // 禁用拷贝
            CommandListWrapper(const CommandListWrapper&)            = delete;
            CommandListWrapper& operator=(const CommandListWrapper&) = delete;

            // 支持移动
            CommandListWrapper(CommandListWrapper&& other) noexcept;
            CommandListWrapper& operator=(CommandListWrapper&& other) noexcept;
        };

        // 命令队列 (从D3D12RenderSystem获取设备，本地管理队列)
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_graphicsQueue; // 图形命令队列 (智能指针)
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_computeQueue; // 计算命令队列 (智能指针)
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_copyQueue; // 复制命令队列 (智能指针)

        // 同步对象 (本类负责围栏管理)
        Microsoft::WRL::ComPtr<ID3D12Fence> m_fence; // 围栏对象 (智能指针)
        uint64_t                            m_currentFenceValue; // 当前围栏值
        HANDLE                              m_fenceEvent; // 围栏事件句柄

        // 命令列表池 - 按类型分组
        std::vector<std::unique_ptr<CommandListWrapper>> m_graphicsCommandLists;
        std::vector<std::unique_ptr<CommandListWrapper>> m_computeCommandLists;
        std::vector<std::unique_ptr<CommandListWrapper>> m_copyCommandLists;

        // 空闲命令列表队列
        std::queue<CommandListWrapper*> m_availableGraphicsLists;
        std::queue<CommandListWrapper*> m_availableComputeLists;
        std::queue<CommandListWrapper*> m_availableCopyLists;

        // 执行中的命令列表 - 用于同步管理
        std::vector<CommandListWrapper*> m_executingLists;

        // 线程安全
        std::mutex m_mutex; // 互斥锁
        bool       m_initialized; // 初始化标记

        // 配置参数
        static constexpr uint32_t DEFAULT_GRAPHICS_LIST_COUNT = 4; // 默认图形命令列表数量
        static constexpr uint32_t DEFAULT_COMPUTE_LIST_COUNT  = 2; // 默认计算命令列表数量
        static constexpr uint32_t DEFAULT_COPY_LIST_COUNT     = 2; // 默认复制命令列表数量

    public:
        /**
         * @brief 构造函数
         * 
         * 教学要点: 构造函数只做基本初始化，实际资源创建在Initialize中
         */
        CommandListManager();

        /**
         * @brief 析构函数
         * 
         * 教学要点: 确保所有GPU操作完成后再释放资源
         */
        ~CommandListManager();

        // ========================================================================
        // 生命周期管理
        // ========================================================================

        /**
         * @brief 初始化命令列表管理器
         * @param graphicsCount 图形命令列表数量
         * @param computeCount 计算命令列表数量
         * @param copyCount 复制命令列表数量
         * @return 成功返回true，失败返回false
         * 
         * 教学要点:
         * 1. 从D3D12RenderSystem::GetDevice()获取设备
         * 2. 创建围栏对象用于CPU/GPU同步
         * 3. 创建三种类型的命令队列
         * 4. 预创建指定数量的命令列表到池中
         * 5. 每个命令列表都有独立的命令分配器
         */
        bool Initialize(uint32_t graphicsCount = DEFAULT_GRAPHICS_LIST_COUNT,
                        uint32_t computeCount  = DEFAULT_COMPUTE_LIST_COUNT,
                        uint32_t copyCount     = DEFAULT_COPY_LIST_COUNT);

        /**
         * @brief 释放所有资源
         * 
         * 教学要点: 等待所有GPU操作完成，然后按依赖关系逆序释放资源
         */
        void Shutdown();

        // ========================================================================
        // 命令列表获取和提交接口
        // ========================================================================

        /**
         * @brief 获取可用的命令列表开始录制
         * @param type 命令列表类型
         * @param debugName 调试名称 (可选)
         * @return 命令列表指针，失败返回nullptr
         * 
         * 教学要点:
         * 1. 从对应类型的空闲队列中取出命令列表
         * 2. 重置命令分配器和命令列表
         * 3. 设置调试名称便于PIX调试
         * 4. 状态设置为Recording
         * 
         * 注意：返回裸指针用于DirectX API调用，但内部使用智能指针管理
         */
        ID3D12GraphicsCommandList* AcquireCommandList(Type type, const std::string& debugName = "");

        /**
         * @brief 提交命令列表执行
         * @param commandList 要提交的命令列表
         * @return 关联的围栏值，用于同步等待
         * 
         * 教学要点:
         * 1. 关闭命令列表 (Close)
         * 2. 提交到对应的命令队列执行
         * 3. 发信号到围栏，标记这批命令的完成时机
         * 4. 状态设置为Executing
         */
        uint64_t ExecuteCommandList(ID3D12GraphicsCommandList* commandList);

        /**
         * @brief 批量提交多个命令列表
         * @param commandLists 命令列表数组
         * @param count 命令列表数量
         * @return 关联的围栏值
         * 
         * 教学要点: 批量提交可以减少API调用开销，提升性能
         */
        uint64_t ExecuteCommandLists(ID3D12GraphicsCommandList* const* commandLists, uint32_t count);

        // ========================================================================
        // 同步管理接口
        // ========================================================================

        /**
         * @brief 等待特定围栏值完成
         * @param fenceValue 要等待的围栏值
         * @param timeoutMs 超时时间 (毫秒)，0表示立即返回，INFINITE表示无限等待
         * @return 等待成功返回true，超时返回false
         * 
         * 教学要点: CPU等待GPU完成特定的命令批次
         */
        bool WaitForFence(uint64_t fenceValue, uint32_t timeoutMs = INFINITE);

        /**
         * @brief 等待GPU完成所有命令
         * @param timeoutMs 超时时间 (毫秒)
         * 
         * 教学要点: 等待所有提交的命令完成，通常在帧结束或退出时使用
         */
        bool WaitForGPU(uint32_t timeoutMs = INFINITE);

        /**
         * @brief 检查围栏值是否已完成
         * @param fenceValue 要检查的围栏值
         * @return 已完成返回true，否则返回false
         * 
         * 教学要点: 非阻塞检查，用于避免不必要的等待
         */
        bool IsFenceCompleted(uint64_t fenceValue) const;

        /**
         * @brief 获取当前已完成的围栏值
         * @return GPU已完成的最大围栏值
         */
        uint64_t GetCompletedFenceValue() const;

        // ========================================================================
        // 资源管理和维护接口
        // ========================================================================

        /**
         * @brief 回收已完成的命令列表
         * 
         * 教学要点:
         * 1. 检查执行中的命令列表，将完成的移回空闲队列
         * 2. 重置命令分配器为下次使用做准备
         * 3. 建议每帧调用一次以保持资源复用
         */
        void UpdateCompletedCommandLists();

        /**
         * @brief 强制回收所有命令列表 (等待GPU完成)
         * 
         * 教学要点: 用于分辨率变更等需要完全同步的场景
         */
        void FlushAllCommandLists();

        // ========================================================================
        // 查询接口
        // ========================================================================

        /**
         * @brief 获取指定类型的可用命令列表数量
         * @param type 命令列表类型
         * @return 可用命令列表数量
         */
        uint32_t GetAvailableCount(Type type) const;

        /**
         * @brief 获取指定类型的执行中命令列表数量
         * @param type 命令列表类型
         * @return 执行中命令列表数量
         */
        uint32_t GetExecutingCount(Type type) const;

        /**
         * @brief 检查管理器是否已初始化
         * @return 已初始化返回true，否则返回false
         */
        bool IsInitialized() const { return m_initialized; }

        /**
         * @brief 获取对应类型的命令队列
         * @param type 命令列表类型
         * @return 命令队列指针 (从智能指针获取裸指针供DirectX API使用)
         * 
         * 教学要点: 内部使用智能指针管理，但返回裸指针供DirectX API调用
         */
        ID3D12CommandQueue* GetCommandQueue(Type type) const;

    private:
        // ========================================================================
        // 私有辅助方法
        // ========================================================================

        /**
         * @brief 创建命令列表池
         * @param type 命令列表类型
         * @param count 创建数量
         * @return 成功返回true，失败返回false
         */
        bool CreateCommandListPool(Type type, uint32_t count);

        /**
         * @brief 创建单个命令列表
         * @param type 命令列表类型
         * @return 创建的命令列表包装器
         */
        std::unique_ptr<CommandListWrapper> CreateCommandList(Type type);

        /**
         * @brief 获取指定类型的空闲队列引用
         * @param type 命令列表类型
         * @return 空闲队列引用
         */
        std::queue<CommandListWrapper*>& GetAvailableQueue(Type type);

        /**
         * @brief 查找命令列表包装器
         * @param commandList 原生命令列表指针
         * @return 包装器指针，未找到返回nullptr
         */
        CommandListWrapper* FindWrapper(ID3D12GraphicsCommandList* commandList);

        /**
         * @brief 获取命令列表类型的字符串名称 (调试用)
         * @param type 命令列表类型
         * @return 类型名称字符串
         */
        static const char* GetTypeName(Type type);

        // 禁用拷贝构造和赋值操作
        CommandListManager(const CommandListManager&)            = delete;
        CommandListManager& operator=(const CommandListManager&) = delete;

        // 支持移动语义 (虽然通常不会移动这个类)
        CommandListManager(CommandListManager&&)            = default;
        CommandListManager& operator=(CommandListManager&&) = default;
    };
} // namespace enigma::graphic
