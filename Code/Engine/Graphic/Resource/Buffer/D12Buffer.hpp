#pragma once
#include "../D12Resources.hpp"

namespace enigma::graphic
{
    /**
 * @brief D12Buffer类 - 专用于延迟渲染的缓冲区封装
 * 
 * 教学要点:
 * 1. 支持顶点缓冲、索引缓冲、常量缓冲、结构化缓冲等
 * 2. 优化的Bindless绑定支持
 * 3. 支持GPU上传和回读操作
 * 4. 集成描述符创建
 */
    class D12Buffer : public D12Resource
    {
    public:
        /**
         * @brief 缓冲区类型枚举
         */
        enum class Type
        {
            Vertex, // 顶点缓冲
            Index, // 索引缓冲
            Constant, // 常量缓冲 (Uniform Buffer)
            Structured, // 结构化缓冲 (SSBO)
            Upload, // 上传缓冲 (CPU -> GPU)
            Readback // 回读缓冲 (GPU -> CPU)
        };

    private:
        Type     m_type; // 缓冲区类型
        uint32_t m_elementCount; // 元素数量
        uint32_t m_elementSize; // 单个元素大小
        uint32_t m_stride; // 步长 (用于结构化缓冲)

        // 描述符句柄
        D3D12_CPU_DESCRIPTOR_HANDLE m_srvHandle; // Shader Resource View
        D3D12_CPU_DESCRIPTOR_HANDLE m_uavHandle; // Unordered Access View
        D3D12_CPU_DESCRIPTOR_HANDLE m_cbvHandle; // Constant Buffer View

        bool m_hasSRV;
        bool m_hasUAV;
        bool m_hasCBV;

        // CPU映射指针 (用于Upload和Readback缓冲)
        void* m_mappedData;

    public:
        /**
         * @brief 构造函数
         */
        D12Buffer();

        /**
         * @brief 析构函数
         */
        virtual ~D12Buffer();

        /**
         * @brief 创建缓冲区
         * @param device DX12设备
         * @param type 缓冲区类型
         * @param elementCount 元素数量
         * @param elementSize 单个元素大小 (字节)
         * @param initialData 初始数据 (可为nullptr)
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: 根据类型自动选择合适的堆类型和资源标志
         */
        bool Create(ID3D12Device* device, Type           type,
                    uint32_t      elementCount, uint32_t elementSize,
                    const void*   initialData = nullptr);

        /**
         * @brief 上传数据到缓冲区
         * @param data 源数据指针
         * @param size 数据大小 (字节)
         * @param offset 偏移量 (字节)
         * @return 成功返回true，失败返回false
         * 
         * 教学要点: 支持部分更新和完整更新
         */
        bool UploadData(const void* data, size_t size, size_t offset = 0);

        /**
         * @brief 从缓冲区读取数据 (仅用于Readback缓冲)
         * @param data 目标数据指针
         * @param size 读取大小 (字节)
         * @param offset 偏移量 (字节)
         * @return 成功返回true，失败返回false
         */
        bool ReadData(void* data, size_t size, size_t offset = 0);

        // ========================================================================
        // 属性访问接口
        // ========================================================================

        Type     GetType() const { return m_type; }
        uint32_t GetElementCount() const { return m_elementCount; }
        uint32_t GetElementSize() const { return m_elementSize; }
        uint32_t GetStride() const { return m_stride; }

        // 专用视图访问 (用于绑定到管线)
        D3D12_VERTEX_BUFFER_VIEW        GetVertexBufferView() const;
        D3D12_INDEX_BUFFER_VIEW         GetIndexBufferView() const;
        D3D12_CONSTANT_BUFFER_VIEW_DESC GetConstantBufferViewDesc() const;

        // 描述符句柄访问
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const { return m_srvHandle; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetUAVHandle() const { return m_uavHandle; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetCBVHandle() const { return m_cbvHandle; }

        bool HasSRV() const { return m_hasSRV; }
        bool HasUAV() const { return m_hasUAV; }
        bool HasCBV() const { return m_hasCBV; }

    private:
        /**
         * @brief 创建描述符视图
         * @param device DX12设备
         */
        void CreateDescriptorViews(ID3D12Device* device);

        /**
         * @brief 根据类型确定堆类型
         * @param type 缓冲区类型
         * @return 堆类型
         */
        static D3D12_HEAP_TYPE GetHeapType(Type type);

        /**
         * @brief 根据类型确定资源标志
         * @param type 缓冲区类型
         * @return 资源标志
         */
        static D3D12_RESOURCE_FLAGS GetResourceFlags(Type type);
    };
}
