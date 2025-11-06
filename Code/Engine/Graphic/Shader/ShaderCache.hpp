/**
 * @file ShaderCache.hpp
 * @brief 统一的着色器缓存管理系统 - 管理ShaderSource和ShaderProgram
 * @date 2025-11-05
 * @author Caizii
 *
 * 架构说明:
 * ShaderCache是RendererSubsystem的核心组件，负责统一管理ShaderSource（持久存储）
 * 和ShaderProgram（可清理缓存）。它提供了分层存储、双重索引和热重载支持。
 *
 * 设计原则:
 * - 统一字符串键: 避免数据冗余和同步问题，所有查找都基于字符串键
 * - 分层存储: ShaderSource持久存储（支持热重载），ShaderProgram可清理（内存管理）
 * - 双重索引: 支持ProgramId和字符串两种查找方式
 * - 热重载支持: 保留ShaderSource用于重新编译，清理ShaderProgram释放内存
 *
 * 职责边界:
 * - + 管理ShaderSource缓存（持久存储，支持热重载）
 * - + 管理ShaderProgram缓存（可清理，内存管理）
 * - + 管理ProgramId到字符串的映射（轻量级映射表）
 * - + 提供统计信息接口
 * - ❌ 不负责着色器编译（委托给DXC编译器）
 * - ❌ 不负责着色器加载（委托给ShaderPackManager）
 * - ❌ 不负责PSO创建（委托给PSOManager）
 *
 * 使用示例:
 * @code
 * // ========== ShaderSource管理示例 ==========
 * 
 * // 1. 缓存ShaderSource（持久存储）
 * ShaderCache cache;
 * auto source = std::make_shared<ShaderSource>("gbuffers_terrain", ...);
 * cache.CacheSource("gbuffers_terrain", source);
 * 
 * // 2. 通过字符串键获取ShaderSource
 * auto retrievedSource = cache.GetSource("gbuffers_terrain");
 * if (retrievedSource) {
 *     std::cout << "Source found: " << retrievedSource->GetName() << std::endl;
 * }
 * 
 * // 3. 通过ProgramId获取ShaderSource
 * cache.RegisterProgramId(ProgramId::Terrain, "gbuffers_terrain");
 * auto sourceById = cache.GetSource(ProgramId::Terrain);
 * if (sourceById) {
 *     std::cout << "Source found by ID: " << sourceById->GetName() << std::endl;
 * }
 * 
 * // ========== ShaderProgram管理示例 ==========
 * 
 * // 4. 缓存ShaderProgram（可清理缓存）
 * auto program = std::make_shared<ShaderProgram>("gbuffers_terrain", ...);
 * cache.CacheProgram("gbuffers_terrain", program);
 * 
 * // 5. 通过字符串键获取ShaderProgram
 * auto retrievedProgram = cache.GetProgram("gbuffers_terrain");
 * if (retrievedProgram) {
 *     std::cout << "Program found: " << retrievedProgram->GetName() << std::endl;
 * }
 * 
 * // 6. 通过ProgramId获取ShaderProgram
 * auto programById = cache.GetProgram(ProgramId::Terrain);
 * if (programById) {
 *     std::cout << "Program found by ID: " << programById->GetName() << std::endl;
 * }
 * 
 * // ========== 热重载示例 ==========
 * 
 * // 7. 清理ShaderProgram缓存（保留ShaderSource用于重新编译）
 * cache.ClearPrograms();
 * std::cout << "Programs cleared, sources retained for hot reload" << std::endl;
 * 
 * // 8. 重新编译（从ShaderSource）
 * auto sourceForReload = cache.GetSource("gbuffers_terrain");
 * if (sourceForReload) {
 *     auto recompiledProgram = CompileShaderProgram(sourceForReload);
 *     cache.CacheProgram("gbuffers_terrain", recompiledProgram);
 * }
 * 
 * // ========== ProgramId映射管理示例 ==========
 * 
 * // 9. 批量注册ProgramId映射
 * std::unordered_map<ProgramId, std::string> mappings = {
 *     {ProgramId::Terrain, "gbuffers_terrain"},
 *     {ProgramId::Water, "gbuffers_water"},
 *     {ProgramId::Entities, "gbuffers_entities"}
 * };
 * cache.RegisterProgramIds(mappings);
 * 
 * // 10. 获取ProgramId对应的程序名称
 * std::string programName = cache.GetProgramName(ProgramId::Terrain);
 * std::cout << "Program name: " << programName << std::endl;
 * 
 * // ========== 统计信息示例 ==========
 * 
 * // 11. 获取缓存统计信息
 * std::cout << "Source count: " << cache.GetSourceCount() << std::endl;
 * std::cout << "Program count: " << cache.GetProgramCount() << std::endl;
 * std::cout << "ProgramId count: " << cache.GetProgramIdCount() << std::endl;
 * @endcode
 *
 * 教学要点:
 * - 缓存管理模式 (Cache Management Pattern)
 * - 分层存储策略 (Layered Storage Strategy)
 * - 双重索引设计 (Dual Index Design)
 * - 热重载支持 (Hot Reload Support)
 * - 智能指针管理 (Smart Pointer Management)
 * - 统一字符串键 (Unified String Key)
 */

#pragma once

#include "ShaderPack/ProgramId.hpp"
#include <memory>
#include <string>
#include <unordered_map>

namespace enigma::graphic
{
    // 前向声明
    class ShaderSource;
    class ShaderProgram;

    /**
     * @class ShaderCache
     * @brief 统一的着色器缓存管理系统
     *
     * 设计理念:
     * - 统一字符串键：所有查找都基于字符串键，避免数据冗余和同步问题
     * - 分层存储：ShaderSource持久存储，ShaderProgram可清理
     * - 双重索引：支持ProgramId和字符串两种查找方式
     * - 热重载支持：保留ShaderSource用于重新编译
     *
     * 核心职责:
     * 1. **ShaderSource管理**：持久存储，支持热重载
     * 2. **ShaderProgram管理**：可清理缓存，内存管理
     * 3. **ProgramId映射管理**：轻量级映射表
     * 4. **统计信息**：提供缓存统计接口
     *
     * 教学要点:
     * - 缓存管理模式 (Cache Management Pattern)
     * - 分层存储策略 (Layered Storage Strategy)
     * - 双重索引设计 (Dual Index Design)
     * - 智能指针共享所有权 (Shared Ownership)
     */
    class ShaderCache
    {
    public:
        // ========================================================================
        // ShaderSource管理（持久存储，支持热重载）
        // ========================================================================

        /**
         * @brief 缓存ShaderSource（持久存储）
         * @param name 着色器源名称（字符串键）
         * @param source ShaderSource智能指针
         *
         * 教学要点:
         * - 持久存储：ShaderSource不会被自动清理
         * - 热重载支持：保留源代码用于重新编译
         * - 覆盖策略：如果已存在同名Source，则覆盖
         *
         * 示例:
         * @code
         * auto source = std::make_shared<ShaderSource>("gbuffers_terrain", ...);
         * cache.CacheSource("gbuffers_terrain", source);
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 如果name已存在，旧的Source会被覆盖
         * - ⚠️ 使用shared_ptr共享所有权
         */
        void CacheSource(const std::string& name, std::shared_ptr<ShaderSource> source);

        /**
         * @brief 通过字符串键获取ShaderSource
         * @param name 着色器源名称
         * @return std::shared_ptr<ShaderSource> ShaderSource智能指针（未找到返回nullptr）
         *
         * 教学要点:
         * - 字符串键查找：统一的查找接口
         * - 空指针安全：未找到时返回nullptr
         *
         * 示例:
         * @code
         * auto source = cache.GetSource("gbuffers_terrain");
         * if (source) {
         *     std::cout << "Source found: " << source->GetName() << std::endl;
         * }
         * @endcode
         */
        std::shared_ptr<ShaderSource> GetSource(const std::string& name) const;

        /**
         * @brief 通过ProgramId获取ShaderSource
         * @param id 程序ID
         * @return std::shared_ptr<ShaderSource> ShaderSource智能指针（未找到返回nullptr）
         *
         * 教学要点:
         * - 双重索引：支持ProgramId查找
         * - 两步查找：ProgramId → 字符串键 → ShaderSource
         *
         * 示例:
         * @code
         * cache.RegisterProgramId(ProgramId::Terrain, "gbuffers_terrain");
         * auto source = cache.GetSource(ProgramId::Terrain);
         * if (source) {
         *     std::cout << "Source found by ID: " << source->GetName() << std::endl;
         * }
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 需要先调用RegisterProgramId注册映射
         * - ⚠️ 如果映射不存在或Source不存在，返回nullptr
         */
        std::shared_ptr<ShaderSource> GetSource(ProgramId id) const;

        /**
         * @brief 检查是否存在指定名称的ShaderSource
         * @param name 着色器源名称
         * @return bool 存在返回true，否则返回false
         *
         * 示例:
         * @code
         * if (cache.HasSource("gbuffers_terrain")) {
         *     std::cout << "Source exists" << std::endl;
         * }
         * @endcode
         */
        bool HasSource(const std::string& name) const;

        /**
         * @brief 移除指定名称的ShaderSource
         * @param name 着色器源名称
         * @return bool 成功移除返回true，未找到返回false
         *
         * 教学要点:
         * - 显式移除：手动清理不需要的Source
         * - 引用计数：如果有其他地方持有shared_ptr，Source不会被立即销毁
         *
         * 示例:
         * @code
         * if (cache.RemoveSource("gbuffers_terrain")) {
         *     std::cout << "Source removed" << std::endl;
         * }
         * @endcode
         */
        bool RemoveSource(const std::string& name);

        /**
         * @brief 清空所有ShaderSource缓存
         *
         * 教学要点:
         * - 批量清理：清空所有Source
         * - 热重载场景：通常不需要清理Source（保留用于重新编译）
         *
         * 示例:
         * @code
         * cache.ClearSources();
         * std::cout << "All sources cleared" << std::endl;
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 这会清空所有Source，通常不推荐使用
         * - ⚠️ 如果需要热重载，应该保留Source
         */
        void ClearSources();

        // ========================================================================
        // ShaderProgram管理（可清理缓存，内存管理）
        // ========================================================================

        /**
         * @brief 缓存ShaderProgram（可清理缓存）
         * @param name 着色器程序名称（字符串键）
         * @param program ShaderProgram智能指针
         *
         * 教学要点:
         * - 可清理缓存：Program可以被清理以释放内存
         * - 覆盖策略：如果已存在同名Program，则覆盖
         *
         * 示例:
         * @code
         * auto program = std::make_shared<ShaderProgram>("gbuffers_terrain", ...);
         * cache.CacheProgram("gbuffers_terrain", program);
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 如果name已存在，旧的Program会被覆盖
         * - ⚠️ 使用shared_ptr共享所有权
         */
        void CacheProgram(const std::string& name, std::shared_ptr<ShaderProgram> program);

        /**
         * @brief 通过字符串键获取ShaderProgram
         * @param name 着色器程序名称
         * @return std::shared_ptr<ShaderProgram> ShaderProgram智能指针（未找到返回nullptr）
         *
         * 教学要点:
         * - 字符串键查找：统一的查找接口
         * - 空指针安全：未找到时返回nullptr
         *
         * 示例:
         * @code
         * auto program = cache.GetProgram("gbuffers_terrain");
         * if (program) {
         *     std::cout << "Program found: " << program->GetName() << std::endl;
         * }
         * @endcode
         */
        std::shared_ptr<ShaderProgram> GetProgram(const std::string& name) const;

        /**
         * @brief 通过ProgramId获取ShaderProgram
         * @param id 程序ID
         * @return std::shared_ptr<ShaderProgram> ShaderProgram智能指针（未找到返回nullptr）
         *
         * 教学要点:
         * - 双重索引：支持ProgramId查找
         * - 两步查找：ProgramId → 字符串键 → ShaderProgram
         *
         * 示例:
         * @code
         * cache.RegisterProgramId(ProgramId::Terrain, "gbuffers_terrain");
         * auto program = cache.GetProgram(ProgramId::Terrain);
         * if (program) {
         *     std::cout << "Program found by ID: " << program->GetName() << std::endl;
         * }
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 需要先调用RegisterProgramId注册映射
         * - ⚠️ 如果映射不存在或Program不存在，返回nullptr
         */
        std::shared_ptr<ShaderProgram> GetProgram(ProgramId id) const;

        /**
         * @brief 检查是否存在指定名称的ShaderProgram
         * @param name 着色器程序名称
         * @return bool 存在返回true，否则返回false
         *
         * 示例:
         * @code
         * if (cache.HasProgram("gbuffers_terrain")) {
         *     std::cout << "Program exists" << std::endl;
         * }
         * @endcode
         */
        bool HasProgram(const std::string& name) const;

        /**
         * @brief 移除指定名称的ShaderProgram
         * @param name 着色器程序名称
         * @return bool 成功移除返回true，未找到返回false
         *
         * 教学要点:
         * - 显式移除：手动清理不需要的Program
         * - 引用计数：如果有其他地方持有shared_ptr，Program不会被立即销毁
         *
         * 示例:
         * @code
         * if (cache.RemoveProgram("gbuffers_terrain")) {
         *     std::cout << "Program removed" << std::endl;
         * }
         * @endcode
         */
        bool RemoveProgram(const std::string& name);

        /**
         * @brief 清空所有ShaderProgram缓存（保留ShaderSource用于热重载）
         *
         * 教学要点:
         * - 批量清理：清空所有Program
         * - 热重载场景：清理Program释放内存，保留Source用于重新编译
         * - 内存管理：释放GPU资源和CPU内存
         *
         * 示例:
         * @code
         * cache.ClearPrograms();
         * std::cout << "All programs cleared, sources retained" << std::endl;
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 这会清空所有Program，但保留Source
         * - ⚠️ 适用于热重载场景
         */
        void ClearPrograms();

        // ========================================================================
        // ProgramId映射管理（轻量级映射表）
        // ========================================================================

        /**
         * @brief 注册ProgramId到字符串的映射
         * @param id 程序ID
         * @param name 着色器程序名称
         *
         * 教学要点:
         * - 双重索引：建立ProgramId到字符串的映射
         * - 轻量级映射：只存储映射关系，不存储实际对象
         *
         * 示例:
         * @code
         * cache.RegisterProgramId(ProgramId::Terrain, "gbuffers_terrain");
         * cache.RegisterProgramId(ProgramId::Water, "gbuffers_water");
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 如果id已存在，旧的映射会被覆盖
         */
        void RegisterProgramId(ProgramId id, const std::string& name);

        /**
         * @brief 获取ProgramId对应的程序名称
         * @param id 程序ID
         * @return std::string 程序名称（未找到返回空字符串）
         *
         * 教学要点:
         * - 反向查找：从ProgramId获取字符串键
         *
         * 示例:
         * @code
         * cache.RegisterProgramId(ProgramId::Terrain, "gbuffers_terrain");
         * std::string name = cache.GetProgramName(ProgramId::Terrain);
         * std::cout << "Program name: " << name << std::endl; // "gbuffers_terrain"
         * @endcode
         *
         * 注意事项:
         * - ⚠️ 如果映射不存在，返回空字符串
         */
        std::string GetProgramName(ProgramId id) const;

        /**
         * @brief 批量注册ProgramId映射
         * @param mappings ProgramId到字符串的映射表
         *
         * 教学要点:
         * - 批量操作：一次性注册多个映射
         * - 便捷接口：简化批量注册流程
         *
         * 示例:
         * @code
         * std::unordered_map<ProgramId, std::string> mappings = {
         *     {ProgramId::Terrain, "gbuffers_terrain"},
         *     {ProgramId::Water, "gbuffers_water"},
         *     {ProgramId::Entities, "gbuffers_entities"}
         * };
         * cache.RegisterProgramIds(mappings);
         * @endcode
         */
        void RegisterProgramIds(const std::unordered_map<ProgramId, std::string>& mappings);

        // ========================================================================
        // 统计信息
        // ========================================================================

        /**
         * @brief 获取ShaderSource缓存数量
         * @return size_t Source数量
         *
         * 示例:
         * @code
         * std::cout << "Source count: " << cache.GetSourceCount() << std::endl;
         * @endcode
         */
        size_t GetSourceCount() const;

        /**
         * @brief 获取ShaderProgram缓存数量
         * @return size_t Program数量
         *
         * 示例:
         * @code
         * std::cout << "Program count: " << cache.GetProgramCount() << std::endl;
         * @endcode
         */
        size_t GetProgramCount() const;

        /**
         * @brief 获取ProgramId映射数量
         * @return size_t 映射数量
         *
         * 示例:
         * @code
         * std::cout << "ProgramId count: " << cache.GetProgramIdCount() << std::endl;
         * @endcode
         */
        size_t GetProgramIdCount() const;

    private:
        /**
         * 教学要点总结：
         * 1. **分层存储策略**：ShaderSource持久存储，ShaderProgram可清理
         * 2. **双重索引设计**：支持字符串键和ProgramId两种查找方式
         * 3. **统一字符串键**：避免数据冗余和同步问题
         * 4. **热重载支持**：保留ShaderSource用于重新编译
         * 5. **智能指针管理**：使用shared_ptr共享所有权
         * 6. **缓存管理模式**：提供完整的缓存管理接口
         * 7. **空指针安全**：所有查找方法都返回智能指针，未找到时返回nullptr
         * 8. **统计信息接口**：提供缓存统计功能
         */

        // ShaderSource缓存（持久存储）
        std::unordered_map<std::string, std::shared_ptr<ShaderSource>> m_sources;

        // ShaderProgram缓存（可清理）
        std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> m_programs;

        // ProgramId到字符串的映射（轻量级映射表）
        std::unordered_map<ProgramId, std::string> m_idToName;
    };
} // namespace enigma::graphic
