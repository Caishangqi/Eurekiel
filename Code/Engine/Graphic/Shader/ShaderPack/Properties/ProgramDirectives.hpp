/**
 * @file ProgramDirectives.hpp
 * @brief 单个Shader Program的渲染指令数据容器
 * @date 2025-10-13
 * @author Caizii
 *
 * 架构说明:
 * ProgramDirectives 存储从着色器注释中解析出的所有渲染配置。
 * 这些配置用于创建 Pipeline State Object (PSO)。
 *
 * 职责边界:
 * - + 存储解析后的数据 (drawBuffers, blendMode, depthTest 等)
 * - + 提供 Getter 访问接口 (供 PSO 创建使用)
 * - ❌ 不负责解析逻辑 (由 CommentDirectiveParser 负责)
 *
 * 对应 Iris:
 * - net.irisshaders.iris.shaderpack.properties.ProgramDirectives
 * - 对应 Iris 的 WorldRenderingSettings (部分功能)
 *
 * 设计理念:
 * - 有状态容器：包含所有渲染配置数据成员
 * - 单一职责：只负责存储和访问，不负责解析
 * - 生命周期：与 ShaderProgram 绑定，长期存在
 * - 对齐 Iris：遵循 Iris 的 Parser-Container 分离原则
 *
 * 教学要点:
 * - 单一职责原则 (SRP): 只负责数据存储，不负责解析
 * - 数据传输对象 (DTO) 模式
 * - 不可变性: 构造后数据不再修改 (只读访问)
 * - 依赖倒置: 依赖 CommentDirectiveParser 的抽象接口 (静态方法)
 *
 * 数据流:
 * CommentDirectiveParser.FindDirective() → CommentDirective (中间数据)
 *   ↓
 * ProgramDirectives 构造函数 → 存储为成员变量
 *   ↓
 * ProgramDirectives.GetXXX() → 提供给 PSO 创建
 *
 * 典型用法:
 * @code
 * ShaderSource source = ...;
 * ProgramDirectives directives(source);
 *
 * // 读取配置
 * auto renderTargets = directives.GetDrawBuffers();
 * auto depthTest = directives.GetDepthTest();
 * @endcode
 *
 * Iris 源码参考:
 * @code{.java}
 * public class ProgramDirectives {
 *     // 数据成员 - 存储解析后的配置
 *     private final int[] drawBuffers;
 *     private final ViewportData viewportScale;
 *     private final AlphaTest alphaTestOverride;
 *     private final Optional<BlendModeOverride> blendModeOverride;
 *
 *     // 构造函数 - 在初始化时完成所有解析
 *     public ProgramDirectives(ProgramSource source, ShaderProperties properties,
 *                             Set<Integer> supportedRenderTargets,
 *                             BlendModeOverride defaultBlendOverride) {
 *         // 调用 CommentDirectiveParser 解析数据
 *         Optional<CommentDirective> directive =
 *             CommentDirectiveParser.findDirective(source.getFragmentSource(), ...);
 *         // 存储解析结果
 *         this.drawBuffers = ...;
 *     }
 * }
 * @endcode
 */

#pragma once

#include "Engine/Graphic/Shader/ShaderPack/Parsing/CommentDirective.hpp"
#include <optional>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdint>

// 前向声明
namespace enigma::graphic
{
    class ShaderSource;
}

namespace enigma::graphic::shader
{
    /**
     * @brief 单个Shader Program的渲染指令数据容器
     *
     * ProgramDirectives 存储从着色器注释中解析出的所有渲染配置。
     * 这些配置用于创建 Pipeline State Object (PSO)。
     *
     * @note 对应 Iris 的 ProgramDirectives.java
     * @note 职责：存储解析后的数据 + 提供 Getter 访问
     * @note 不负责：解析逻辑 (由 CommentDirectiveParser 负责)
     *
     * 设计理念:
     * - 有状态容器：包含所有渲染配置数据成员
     * - 单一职责：只负责存储和访问，不负责解析
     * - 生命周期：与 ShaderProgram 绑定，长期存在
     * - 对齐 Iris：遵循 Iris 的 Parser-Container 分离原则
     *
     * 数据流:
     * CommentDirectiveParser.FindDirective() → CommentDirective (中间数据)
     *   ↓
     * ProgramDirectives 构造函数 → 存储为成员变量
     *   ↓
     * ProgramDirectives.GetXXX() → 提供给 PSO 创建
     *
     * 典型用法:
     * @code
     * ShaderSource source = ...;
     * ProgramDirectives directives(source);
     *
     * // 读取配置
     * auto renderTargets = directives.GetDrawBuffers();
     * auto depthTest = directives.GetDepthTest();
     * @endcode
     */
    class ProgramDirectives
    {
    public:
        /**
         * @brief 默认构造函数 - 应用默认配置
         *
         * 创建一个使用默认值的 ProgramDirectives 对象。
         * 用于需要延迟初始化的场景 (如 ShaderSource, BuildResult)。
         *
         * 默认值:
         * - drawBuffers: {0} (只输出到 RT0)
         * - 其他指令: std::nullopt (未指定)
         *
         * @note 构造后可以通过赋值操作符重新初始化
         *
         * 教学要点:
         * - 延迟初始化模式: 允许先构造对象，再设置实际值
         * - 默认值语义: 未指定指令时的安全行为
         * - 解决构造顺序问题: 避免成员初始化列表中使用 this 指针
         *
         * 典型用法:
         * @code
         * // 1. 默认构造 (无需着色器源码)
         * ProgramDirectives directives;
         *
         * // 2. 稍后重新初始化
         * directives = ProgramDirectives(shaderSource);
         * @endcode
         */
        ProgramDirectives();

        /**
         * @brief 构造函数 - 解析着色器源码中的所有指令
         *
         * 在构造时调用 CommentDirectiveParser 解析所有指令，
         * 并存储为成员变量。
         *
         * @param source 着色器程序源码 (包含 VS, PS, GS 等)
         *
         * @note 在构造函数中完成所有解析工作 (对齐 Iris)
         *
         * 构造过程:
         * 1. 调用 CommentDirectiveParser 解析注释指令
         * 2. 调用 ConstDirectiveParser 解析常量指令 (TODO)
         * 3. 从 ShaderProperties 加载外部配置 (TODO)
         * 4. 存储所有解析结果
         *
         * Iris 构造函数签名:
         * @code{.java}
         * public ProgramDirectives(ProgramSource source, ShaderProperties properties,
         *                         Set<Integer> supportedRenderTargets,
         *                         BlendModeOverride defaultBlendOverride)
         * @endcode
         */
        explicit ProgramDirectives(const enigma::graphic::ShaderSource& source);

        // ========== Getter 方法 (只读访问) ==========

        /**
         * @brief 获取 DrawBuffers 列表
         *
         * DrawBuffers 指令指定片段着色器输出到哪些渲染目标。
         * 例如: DRAWBUFFERS:01 表示输出到 RT0 和 RT1。
         *
         * @return const std::vector<uint32_t>& 渲染目标索引列表
         *
         * @note 默认值：{0} (只输出到 RT0)
         *
         * 示例:
         * @code
         * // DRAWBUFFERS:01 解析为 {0, 1, 5, 7}
         * auto rts = directives.GetDrawBuffers();
         * for (uint32_t index : rts) {
         *     psoDesc.RTFormats[index] = DXGI_FORMAT_R16G16B16A16_FLOAT;
         * }
         * @endcode
         *
         * Iris 对应:
         * @code{.java}
         * public int[] getDrawBuffers() { return drawBuffers; }
         * @endcode
         */
        const std::vector<uint32_t>& GetDrawBuffers() const { return m_drawBuffers; }

        /**
         * @brief 获取 BlendMode
         *
         * BlendMode 指令指定混合模式。
         * 例如: BLEND:ADD 表示加法混合。
         *
         * @return std::optional<std::string> 混合模式 (如果指定)
         *
         * 支持的混合模式:
         * - ADD: 加法混合
         * - MULTIPLY: 乘法混合
         * - SUBTRACT: 减法混合
         * - ALPHA: Alpha 混合 (默认)
         *
         * 示例:
         * @code
         * auto blendMode = directives.GetBlendMode();
         * if (blendMode) {
         *     if (*blendMode == "ADD") {
         *         psoDesc.BlendState = AddBlendState();
         *     }
         * }
         * @endcode
         */
        std::optional<std::string> GetBlendMode() const { return m_blendMode; }

        /**
         * @brief 获取 DepthTest
         *
         * DepthTest 指令指定深度测试模式。
         * 例如: DEPTHTEST:GREATER 表示使用 GREATER 深度测试。
         *
         * @return std::optional<std::string> 深度测试模式 (如果指定)
         *
         * 支持的深度测试模式:
         * - LESS: 深度小于时通过 (默认)
         * - LEQUAL: 深度小于或等于时通过
         * - GREATER: 深度大于时通过
         * - GEQUAL: 深度大于或等于时通过
         * - EQUAL: 深度相等时通过
         * - NOTEQUAL: 深度不等时通过
         * - ALWAYS: 始终通过
         * - NEVER: 从不通过
         */
        std::optional<std::string> GetDepthTest() const { return m_depthTest; }

        /**
         * @brief 获取 CullFace
         *
         * CullFace 指令指定面剔除模式。
         * 例如: CULLFACE:BACK 表示剔除背面。
         *
         * @return std::optional<std::string> 面剔除模式 (如果指定)
         *
         * 支持的面剔除模式:
         * - NONE: 不剔除
         * - FRONT: 剔除正面
         * - BACK: 剔除背面 (默认)
         */
        std::optional<std::string> GetCullFace() const { return m_cullFace; }

        /**
         * @brief 获取 DepthWrite
         *
         * DepthWrite 指令指定是否写入深度缓冲。
         * 例如: DEPTHWRITE:OFF 表示关闭深度写入。
         *
         * @return std::optional<bool> 深度写入开关 (如果指定)
         *
         * 支持的值:
         * - ON / TRUE: 开启深度写入 (默认)
         * - OFF / FALSE: 关闭深度写入
         */
        std::optional<bool> GetDepthWrite() const { return m_depthWrite; }

        /**
         * @brief 获取 AlphaTest 阈值
         *
         * AlphaTest 指令指定 Alpha 测试阈值。
         * 例如: ALPHATEST:0.5 表示 Alpha < 0.5 时丢弃片段。
         *
         * @return std::optional<float> Alpha 测试阈值 (如果指定)
         *
         * 阈值范围: 0.0 - 1.0
         */
        std::optional<float> GetAlphaTest() const { return m_alphaTest; }

        /**
         * @brief 获取 RenderTarget 格式覆盖
         *
         * FORMAT 指令可以覆盖特定 RT 的格式。
         * 例如: FORMAT:0:RGBA16F 表示 RT0 使用 RGBA16F 格式。
         *
         * @return const std::unordered_map<std::string, std::string>& RT 格式映射
         *
         * 映射关系:
         * - Key: RT 索引字符串 (例如 "0", "1", "2")
         * - Value: 格式字符串 (例如 "RGBA16F", "RGBA32F")
         *
         * 示例:
         * @code
         * auto rtFormats = directives.GetRTFormats();
         * if (rtFormats.count("0")) {
         *     std::string format = rtFormats.at("0");  // "RGBA16F"
         * }
         * @endcode
         */
        const std::unordered_map<std::string, std::string>& GetRTFormats() const { return m_rtFormats; }

    private:
        // ========== 数据成员 (存储解析后的配置) ==========

        std::vector<uint32_t>                        m_drawBuffers; ///< DrawBuffers 列表 (默认: {0})
        std::optional<std::string>                   m_blendMode; ///< 混合模式 (默认: std::nullopt)
        std::optional<std::string>                   m_depthTest; ///< 深度测试 (默认: std::nullopt)
        std::optional<std::string>                   m_cullFace; ///< 面剔除 (默认: std::nullopt)
        std::optional<bool>                          m_depthWrite; ///< 深度写入 (默认: std::nullopt)
        std::optional<float>                         m_alphaTest; ///< Alpha 测试 (默认: std::nullopt)
        std::unordered_map<std::string, std::string> m_rtFormats; ///< RT 格式覆盖

        // ========== 内部解析方法 ==========

        /**
         * @brief 解析注释指令
         *
         * 调用 CommentDirectiveParser 解析所有注释指令。
         *
         * @param fragmentSource 片段着色器源码
         *
         * 解析的指令:
         * - DRAWBUFFERS / RENDERTARGETS
         * - BLEND
         * - DEPTHTEST
         * - CULLFACE
         * - DEPTHWRITE
         * - ALPHATEST
         * - FORMAT
         */
        void ParseCommentDirectives(const std::string& fragmentSource);

        /**
         * @brief 应用默认值
         *
         * 对未指定的指令应用 Iris 兼容的默认值。
         *
         * 默认值:
         * - drawBuffers: {0} (只输出到 RT0)
         * - 其他指令: std::nullopt (未指定)
         */
        void ApplyDefaults();
    };
} // namespace enigma::graphic::shader
