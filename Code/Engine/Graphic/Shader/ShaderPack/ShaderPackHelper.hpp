/**
 * @file ShaderPackHelper.hpp
 * @brief ShaderPack系统辅助工具类 - 统一管理ShaderPack相关工具函数
 * @date 2025-11-05
 * @author Caizii
 *
 * 架构说明:
 * ShaderPackHelper 是一个纯静态工具类，为ShaderPack系统提供便捷的工具函数接口。
 * 它整合了路径选择、结构验证、Fallback机制和加载逻辑等常用功能，简化ShaderPack系统的使用。
 *
 * 设计原则:
 * - 纯静态类: 所有函数都是static，禁止实例化
 * - 统一接口: 为ShaderPack系统提供一致的工具函数访问方式
 * - 便捷封装: 简化常用操作的实现复杂度
 * - 功能分组: 按功能域组织函数（路径选择、结构验证、Fallback机制、加载逻辑）
 *
 * 职责边界:
 * - + 路径选择工具 (SelectShaderPackPath)
 * - + 结构验证工具 (ValidateShaderPackStructure)
 * - + 双ShaderPack Fallback机制 (GetShaderProgramWithFallback)
 * - + ShaderPack加载逻辑 (LoadShaderPackFromPath)
 * - ❌ 不负责具体的文件IO实现 (委托给IFileReader)
 * - ❌ 不负责ShaderPack的生命周期管理 (委托给ShaderPackManager)
 * - ❌ 不负责着色器编译 (委托给DXC编译器)
 *
 * 使用示例:
 * @code
 * // ========== 路径选择示例 ==========
 * 
 * // 1. 选择用户ShaderPack路径（优先用户包，回退到引擎默认包）
 * std::filesystem::path userPackPath = "F:/shaderpacks/MyPack";
 * std::filesystem::path enginePackPath = "F:/Engine/DefaultShaders";
 * std::filesystem::path selectedPath = ShaderPackHelper::SelectShaderPackPath(userPackPath, enginePackPath);
 * // selectedPath = "F:/shaderpacks/MyPack" (如果MyPack有效)
 * // selectedPath = "F:/Engine/DefaultShaders" (如果MyPack无效)
 * 
 * // ========== 结构验证示例 ==========
 * 
 * // 2. 验证ShaderPack目录结构
 * bool isValid = ShaderPackHelper::ValidateShaderPackStructure("F:/shaderpacks/MyPack");
 * if (!isValid) {
 *     std::cerr << "Invalid ShaderPack structure" << std::endl;
 * }
 * 
 * // ========== 双ShaderPack Fallback示例 ==========
 * 
 * // 3. 从双ShaderPack获取着色器程序（用户包 → 引擎默认包）
 * ShaderPack* userPack = ...; // 用户ShaderPack
 * ShaderPack* enginePack = ...; // 引擎默认ShaderPack
 * ProgramId programId = ProgramId::Terrain;
 * 
 * ShaderProgram* program = ShaderPackHelper::GetShaderProgramWithFallback(
 *     userPack, enginePack, programId
 * );
 * if (program) {
 *     // 使用着色器程序...
 * }
 * 
 * // ========== ShaderPack加载示例 ==========
 * 
 * // 4. 从路径加载ShaderPack
 * auto shaderPack = ShaderPackHelper::LoadShaderPackFromPath("F:/shaderpacks/MyPack");
 * if (shaderPack) {
 *     std::cout << "ShaderPack loaded successfully" << std::endl;
 *     std::cout << "Root: " << shaderPack->GetRootPath() << std::endl;
 * } else {
 *     std::cerr << "Failed to load ShaderPack" << std::endl;
 * }
 * 
 * // ========== 完整使用流程示例 ==========
 * 
 * // 完整流程：从路径选择到程序获取
 * std::filesystem::path userPackPath = "F:/shaderpacks/MyPack";
 * std::filesystem::path enginePackPath = "F:/Engine/DefaultShaders";
 * 
 * // 1. 选择有效的ShaderPack路径
 * std::filesystem::path selectedPath = ShaderPackHelper::SelectShaderPackPath(
 *     userPackPath, enginePackPath
 * );
 * 
 * // 2. 加载用户ShaderPack
 * auto userPack = ShaderPackHelper::LoadShaderPackFromPath(selectedPath);
 * if (!userPack) {
 *     std::cerr << "Failed to load user ShaderPack" << std::endl;
 *     return;
 * }
 * 
 * // 3. 加载引擎默认ShaderPack（作为Fallback）
 * auto enginePack = ShaderPackHelper::LoadShaderPackFromPath(enginePackPath);
 * 
 * // 4. 获取着色器程序（双ShaderPack Fallback）
 * ProgramId programId = ProgramId::Terrain;
 * ShaderProgram* program = ShaderPackHelper::GetShaderProgramWithFallback(
 *     userPack.get(), enginePack.get(), programId
 * );
 * 
 * if (program) {
 *     // 使用着色器程序进行渲染...
 * }
 * @endcode
 *
 * 教学要点:
 * - 工具类模式 (Utility Class Pattern)
 * - 静态方法设计 (Static Method Design)
 * - 便捷函数封装 (Convenience Function Wrapper)
 * - 双ShaderPack Fallback机制 (Dual ShaderPack Fallback)
 * - ShaderPack架构 (ShaderPack Architecture)
 */

#pragma once

#include "ShaderPack.hpp"
#include "ProgramId.hpp"
#include <filesystem>
#include <memory>

namespace enigma::graphic
{
    // 前向声明
    class ShaderProgram;

    /**
     * @class ShaderPackHelper
     * @brief ShaderPack系统辅助工具类 - 纯静态工具类
     *
     * 设计理念:
     * - 纯静态类：所有方法都是static，禁止实例化
     * - 功能分组：按职责域组织（路径选择、结构验证、Fallback机制、加载逻辑）
     * - 便捷封装：简化ShaderPack系统的常用操作
     * - 统一接口：为ShaderPack系统提供一致的工具函数访问方式
     *
     * 功能分组:
     * 1. **路径选择函数**：
     *    - SelectShaderPackPath: 选择有效的ShaderPack路径（用户包 → 引擎默认包）
     *
     * 2. **结构验证函数**：
     *    - ValidateShaderPackStructure: 验证ShaderPack目录结构是否有效
     *
     * 3. **双ShaderPack Fallback机制**：
     *    - GetShaderProgramWithFallback: 从双ShaderPack获取着色器程序（用户包 → 引擎默认包）
     *
     * 4. **ShaderPack加载函数**：
     *    - LoadShaderPackFromPath: 从路径加载ShaderPack
     *
     * 教学要点:
     * - 工具类模式 (Utility Class Pattern)
     * - 静态方法设计 (Static Method Design)
     * - 依赖注入原则 (Dependency Injection Principle)
     * - 门面模式 (Facade Pattern)
     * - RAII资源管理
     * - 双ShaderPack Fallback机制
     */
    class ShaderPackHelper
    {
    public:
        // 禁止实例化 (纯静态工具类)
        ShaderPackHelper()                                   = delete;
        ~ShaderPackHelper()                                  = delete;
        ShaderPackHelper(const ShaderPackHelper&)            = delete;
        ShaderPackHelper& operator=(const ShaderPackHelper&) = delete;

        // ========================================================================
        // 路径选择函数组
        // ========================================================================

        /**
         * @brief 选择有效的ShaderPack路径（用户包 → 引擎默认包）
         * @param userPackPath 用户ShaderPack路径
         * @param enginePackPath 引擎默认ShaderPack路径
         * @return std::filesystem::path 选择的有效路径
         *
         * 教学要点:
         * - 双ShaderPack架构：用户包优先，引擎默认包作为Fallback
         * - 路径验证：自动验证路径有效性
         * - 容错处理：如果用户包无效，自动回退到引擎默认包
         *
         * 选择策略:
         * 1. 如果userPackPath有效 → 返回userPackPath
         * 2. 否则 → 返回enginePackPath
         * 3. 路径有效性检查：
         *    - 路径存在
         *    - 是目录
         *    - 包含shaders/子目录
         *
         * 示例:
         * @code
         * // 用户包有效
         * std::filesystem::path userPath = "F:/shaderpacks/MyPack";
         * std::filesystem::path enginePath = "F:/Engine/DefaultShaders";
         * std::filesystem::path selected = ShaderPackHelper::SelectShaderPackPath(userPath, enginePath);
         * // selected = "F:/shaderpacks/MyPack"
         *
         * // 用户包无效（不存在）
         * std::filesystem::path invalidPath = "F:/shaderpacks/NonExistent";
         * std::filesystem::path selected2 = ShaderPackHelper::SelectShaderPackPath(invalidPath, enginePath);
         * // selected2 = "F:/Engine/DefaultShaders"
         *
         * // 用户包无效（不是目录）
         * std::filesystem::path filePath = "F:/shaderpacks/MyPack.zip";
         * std::filesystem::path selected3 = ShaderPackHelper::SelectShaderPackPath(filePath, enginePath);
         * // selected3 = "F:/Engine/DefaultShaders"
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 如果两个路径都无效，返回enginePackPath（调用者需要进一步验证）
         * - ⚠️ 这是一个启发式检查，不保证ShaderPack完全有效
         * - ⚠️ 建议配合ValidateShaderPackStructure()进行完整验证
         */
        static std::filesystem::path SelectShaderPackPath(
            const std::filesystem::path& userPackPath,
            const std::filesystem::path& enginePackPath
        );

        // ========================================================================
        // 结构验证函数组
        // ========================================================================

        /**
         * @brief 验证ShaderPack目录结构是否有效
         * @param packPath ShaderPack根目录路径
         * @return bool 结构有效返回true，否则返回false
         *
         * 教学要点:
         * - 结构验证：检查ShaderPack目录是否符合Iris标准
         * - 必需文件检查：验证关键目录和文件是否存在
         * - 容错处理：缺少可选文件不影响验证结果
         *
         * 验证规则（Iris标准）:
         * 1. **必需目录**：
         *    - shaders/ 目录必须存在
         *
         * 2. **必需文件**（至少一个）：
         *    - shaders/gbuffers_basic.vs.hlsl 或 shaders/gbuffers_basic.vsh
         *    - shaders/gbuffers_basic.ps.hlsl 或 shaders/gbuffers_basic.fsh
         *    （Basic程序是最基础的程序，所有其他程序都可以回退到它）
         *
         * 3. **可选文件**（不影响验证）：
         *    - shaders.properties（配置文件）
         *    - shaders/lib/（库文件目录）
         *    - shaders/world0/（维度覆盖目录）
         *
         * 示例:
         * @code
         * // 有效的ShaderPack
         * bool valid1 = ShaderPackHelper::ValidateShaderPackStructure("F:/shaderpacks/MyPack");
         * // valid1 = true (包含shaders/目录和Basic程序)
         *
         * // 无效的ShaderPack（缺少shaders/目录）
         * bool valid2 = ShaderPackHelper::ValidateShaderPackStructure("F:/shaderpacks/EmptyPack");
         * // valid2 = false
         *
         * // 无效的ShaderPack（缺少Basic程序）
         * bool valid3 = ShaderPackHelper::ValidateShaderPackStructure("F:/shaderpacks/IncompletePack");
         * // valid3 = false
         * @endcode
         *
         * 验证流程:
         * 1. 检查packPath是否存在且是目录
         * 2. 检查shaders/子目录是否存在
         * 3. 检查是否存在Basic程序文件（.vs.hlsl/.vsh 和 .ps.hlsl/.fsh）
         * 4. 所有检查通过 → 返回true
         * 5. 任何检查失败 → 返回false
         *
         * 注意事项:
         * - ⚠️ 这是一个基础验证，不检查文件内容是否正确
         * - ⚠️ 不检查着色器语法错误
         * - ⚠️ 不检查Include依赖是否完整
         * - ⚠️ 建议在加载ShaderPack后进一步验证IsValid()
         */
        static bool ValidateShaderPackStructure(const std::filesystem::path& packPath);

        // ========================================================================
        // 双ShaderPack Fallback机制
        // ========================================================================

        /**
         * @brief 从双ShaderPack获取着色器程序（用户包 → 引擎默认包）
         * @param userPack 用户ShaderPack指针（可以为nullptr）
         * @param enginePack 引擎默认ShaderPack指针（可以为nullptr）
         * @param programId 程序ID
         * @return ShaderProgram* 着色器程序指针（如果两个包都没有则返回nullptr）
         *
         * 教学要点:
         * - 双ShaderPack Fallback机制：用户包优先，引擎默认包作为Fallback
         * - Iris Fallback Chain：程序级别的回退链（如Terrain → TexturedLit → Textured → Basic）
         * - 三级Fallback策略：用户包程序 → 用户包回退 → 引擎默认包程序 → 引擎默认包回退
         * - 空指针安全：处理userPack或enginePack为nullptr的情况
         *
         * Fallback流程（三级Fallback）:
         * 1. **用户包程序查找**：
         *    - 如果userPack非空 → 查找programId对应的程序
         *    - 如果找到 → 返回该程序
         *
         * 2. **用户包Fallback Chain**：
         *    - 如果用户包没有programId → 沿着Fallback Chain查找
         *    - 例如：Terrain → TexturedLit → Textured → Basic
         *    - 如果找到 → 返回该程序
         *
         * 3. **引擎默认包程序查找**：
         *    - 如果用户包完全没有 → 查找enginePack的programId
         *    - 如果找到 → 返回该程序
         *
         * 4. **引擎默认包Fallback Chain**：
         *    - 如果引擎默认包没有programId → 沿着Fallback Chain查找
         *    - 如果找到 → 返回该程序
         *
         * 5. **最终失败**：
         *    - 如果两个包都没有 → 返回nullptr
         *
         * 示例:
         * @code
         * ShaderPack* userPack = ...; // 用户ShaderPack
         * ShaderPack* enginePack = ...; // 引擎默认ShaderPack
         *
         * // 场景1：用户包有Terrain程序
         * ShaderProgram* program1 = ShaderPackHelper::GetShaderProgramWithFallback(
         *     userPack, enginePack, ProgramId::Terrain
         * );
         * // program1 = userPack->GetProgramSet()->GetProgram(ProgramId::Terrain)
         *
         * // 场景2：用户包没有Terrain，但有TexturedLit（Fallback）
         * ShaderProgram* program2 = ShaderPackHelper::GetShaderProgramWithFallback(
         *     userPack, enginePack, ProgramId::Terrain
         * );
         * // program2 = userPack->GetProgramSet()->GetProgram(ProgramId::TexturedLit)
         *
         * // 场景3：用户包完全没有，使用引擎默认包
         * ShaderProgram* program3 = ShaderPackHelper::GetShaderProgramWithFallback(
         *     userPack, enginePack, ProgramId::Terrain
         * );
         * // program3 = enginePack->GetProgramSet()->GetProgram(ProgramId::Terrain)
         *
         * // 场景4：用户包为nullptr，直接使用引擎默认包
         * ShaderProgram* program4 = ShaderPackHelper::GetShaderProgramWithFallback(
         *     nullptr, enginePack, ProgramId::Terrain
         * );
         * // program4 = enginePack->GetProgramSet()->GetProgram(ProgramId::Terrain)
         * @endcode
         *
         * Iris Fallback Chain参考:
         * - Terrain → TexturedLit → Textured → Basic
         * - TerrainCutout → Terrain → TexturedLit → Textured → Basic
         * - Entities → TexturedLit → Textured → Basic
         * - Shadow → nullptr（Shadow没有Fallback）
         *
         * 注意事项:
         * - ⚠️ userPack和enginePack都可以为nullptr（空指针安全）
         * - ⚠️ 如果两个包都为nullptr，返回nullptr
         * - ⚠️ 如果两个包都没有programId及其Fallback，返回nullptr
         * - ⚠️ 调用者需要检查返回值是否为nullptr
         */
        static ShaderProgram* GetShaderProgramWithFallback(
            ShaderPack* userPack,
            ShaderPack* enginePack,
            ProgramId   programId
        );

        // ========================================================================
        // ShaderPack加载函数组
        // ========================================================================

        /**
         * @brief 选择ShaderPack路径（智能路径选择）
         * @param currentPackName 当前ShaderPack名称
         * @param userSearchPath 用户搜索路径
         * @param engineDefaultPath 引擎默认路径
         * @return std::string 选择的ShaderPack路径
         *
         * 教学要点:
         * - 智能路径选择：根据currentPackName和搜索路径自动选择有效路径
         * - 双ShaderPack架构：用户包优先，引擎默认包作为Fallback
         * - 路径验证：自动验证路径有效性
         *
         * 选择策略:
         * 1. 如果currentPackName非空 → 在userSearchPath中查找
         * 2. 如果找到且有效 → 返回用户包路径
         * 3. 否则 → 返回engineDefaultPath
         *
         * 示例:
         * @code
         * std::string packName = "MyPack";
         * std::string userPath = "F:/shaderpacks";
         * std::string enginePath = "F:/Engine/DefaultShaders";
         * std::string selected = ShaderPackHelper::SelectShaderPackPath(packName, userPath, enginePath);
         * // selected = "F:/shaderpacks/MyPack" (如果MyPack有效)
         * // selected = "F:/Engine/DefaultShaders" (如果MyPack无效)
         * @endcode
         */
        static std::string SelectShaderPackPath(
            const std::string& currentPackName,
            const std::string& userSearchPath,
            const std::string& engineDefaultPath
        );

        /**
         * @brief 从路径加载ShaderPack（封装加载逻辑）
         * @param packPath ShaderPack路径
         * @param shaderCache ShaderCache指针（用于缓存ShaderSource）
         * @return std::shared_ptr<ShaderPack> 加载的ShaderPack（失败返回nullptr）
         *
         * 教学要点:
         * - 便捷接口：简化ShaderPack加载流程
         * - ShaderCache集成：自动缓存ShaderSource到ShaderCache
         * - 错误处理：加载失败时返回nullptr
         * - RAII资源管理：使用shared_ptr管理ShaderPack生命周期
         *
         * 加载流程:
         * 1. 验证packPath是否有效
         * 2. 创建ShaderPack对象
         * 3. 验证ShaderPack是否成功加载
         * 4. 如果shaderCache非空，缓存ShaderSource
         * 5. 返回ShaderPack（失败返回nullptr）
         *
         * 示例:
         * @code
         * ShaderCache* cache = GetShaderCache();
         * auto pack = ShaderPackHelper::LoadShaderPackFromPath("F:/shaderpacks/MyPack", cache);
         * if (pack) {
         *     std::cout << "ShaderPack loaded and cached successfully" << std::endl;
         * }
         * @endcode
         */
        static std::shared_ptr<ShaderPack> LoadShaderPackFromPath(
            const std::filesystem::path& packPath,
            class ShaderCache*           shaderCache
        );

        /**
         * @brief 从路径加载ShaderPack
         * @param packPath ShaderPack根目录路径
         * @return std::unique_ptr<ShaderPack> 加载的ShaderPack（失败返回nullptr）
         *
         * 教学要点:
         * - 便捷接口：简化ShaderPack加载流程
         * - 错误处理：加载失败时返回nullptr
         * - RAII资源管理：使用unique_ptr管理ShaderPack生命周期
         * - 异常安全：捕获所有异常并返回nullptr
         *
         * 加载流程:
         * 1. 验证packPath是否有效（使用ValidateShaderPackStructure）
         * 2. 创建ShaderPack对象（调用ShaderPack构造函数）
         * 3. 验证ShaderPack是否成功加载（调用IsValid()）
         * 4. 返回ShaderPack（失败返回nullptr）
         *
         * 示例:
         * @code
         * // 加载用户ShaderPack
         * auto userPack = ShaderPackHelper::LoadShaderPackFromPath("F:/shaderpacks/MyPack");
         * if (userPack) {
         *     std::cout << "User ShaderPack loaded successfully" << std::endl;
         *     std::cout << "Root: " << userPack->GetRootPath() << std::endl;
         *     std::cout << "Valid: " << userPack->IsValid() << std::endl;
         * } else {
         *     std::cerr << "Failed to load user ShaderPack" << std::endl;
         * }
         *
         * // 加载引擎默认ShaderPack
         * auto enginePack = ShaderPackHelper::LoadShaderPackFromPath("F:/Engine/DefaultShaders");
         * if (!enginePack) {
         *     std::cerr << "Failed to load engine default ShaderPack" << std::endl;
         *     // 这是致命错误，引擎默认包必须存在
         *     return;
         * }
         * @endcode
         *
         * 失败原因:
         * - packPath不存在
         * - packPath不是目录
         * - 缺少shaders/子目录
         * - 缺少Basic程序文件
         * - IncludeGraph构建失败（循环依赖）
         * - ShaderProperties解析失败
         * - ProgramSet加载失败
         *
         * 注意事项:
         * - ⚠️ 加载失败时返回nullptr，调用者需要检查
         * - ⚠️ 可能抛出异常（已捕获并返回nullptr）
         * - ⚠️ 加载过程可能耗时较长（扫描文件、构建依赖图）
         * - ⚠️ 建议在后台线程加载ShaderPack
         */
        static std::unique_ptr<ShaderPack> LoadShaderPackFromPath(
            const std::filesystem::path& packPath
        );

    private:
        /**
         * 教学要点总结：
         * 1. **工具类模式**：纯静态类，所有方法都是static，禁止实例化
         * 2. **功能分组设计**：按职责域组织（路径选择、结构验证、Fallback机制、加载逻辑）
         * 3. **便捷接口封装**：简化常用操作的实现复杂度
         * 4. **双ShaderPack Fallback机制**：用户包优先，引擎默认包作为Fallback
         * 5. **三级Fallback策略**：用户包程序 → 用户包回退 → 引擎默认包程序 → 引擎默认包回退
         * 6. **错误处理策略**：使用返回值而非异常处理预期错误情况
         * 7. **空指针安全**：处理userPack或enginePack为nullptr的情况
         * 8. **RAII资源管理**：使用智能指针管理动态资源
         * 9. **Iris兼容性**：遵循Iris的Fallback Chain和目录结构标准
         * 10. **教学导向**：每个方法都有详细的Doxygen注释和教学要点
         */
    };
} // namespace enigma::graphic
