/**
 * @file TestPSO.hpp
 * @brief 临时PSO管理器用于三角形绘制测试
 * @date 2025-10-21
 *
 * TODO(M3): Replace with PipelineManager system
 * 这是临时实现，仅用于验证PSO创建流程
 * 正式实现应该使用PipelineManager自动管理PSO
 *
 * 教学要点:
 * - 静态工具类封装PSO创建逻辑
 * - 使用DXCCompiler编译硬编码Shader
 * - 配置Graphics Pipeline State
 * - 详细日志记录每个步骤
 */

#pragma once

#include <wrl/client.h>
#include <d3d12.h>

namespace enigma::graphic::test
{
    /**
     * @brief 测试PSO管理器（静态工具类）
     *
     * 职责:
     * - 创建三角形测试用的Graphics PSO
     * - 编译硬编码Shader
     * - 配置固定的PSO状态
     *
     * 教学要点:
     * - 静态工具类模式（无状态，纯函数式）
     * - 每次调用都重新创建PSO（测试用）
     * - 详细日志帮助调试
     */
    class TestPSOManager
    {
    public:
        /**
         * @brief 创建三角形测试PSO
         *
         * 流程:
         * 1. 编译顶点着色器 (VSMain, vs_6_6)
         * 2. 编译像素着色器 (PSMain, ps_6_6)
         * 3. 配置Input Layout (Vertex_PCUTBN)
         * 4. 配置RTV格式 (R8G8B8A8_UNORM)
         * 5. 禁用深度测试（测试简化）
         * 6. 创建PSO
         *
         * @return PSO对象（失败返回nullptr）
         *
         * 教学要点:
         * - 使用Bindless RootSignature（全局共享）
         * - 编译失败会有详细错误信息
         * - PSO创建失败会记录日志
         */
        static Microsoft::WRL::ComPtr<ID3D12PipelineState> CreateTrianglePSO();

    private:
        TestPSOManager() = delete; // 禁止实例化
    };
} // namespace enigma::graphic::test
