#pragma once
//-----------------------------------------------------------------------------------------------
// UserDefinedBundle.hpp
//
// [NEW] User-defined shader bundle for managing shaders in bundle/{name}/ directory
//
// This class provides:
//   - Precompilation of all shader programs during bundle load
//   - Single program query by name (GetProgram)
//   - Batch program query by regex pattern (GetPrograms)
//   - Program existence check (HasProgram)
//
// Design Principles (SOLID + KISS):
//   - Single Responsibility: Only manages user shader bundle programs
//   - Resilient Loading: Failed program compilations log warnings, don't fail entire load
//   - Cache-based Access: Programs stored as shared_ptr, returns raw pointer (no ownership transfer)
//   - Stateless Queries: GetProgram/GetPrograms return nullptr/empty if not found (no exceptions)
//
// Ownership Model:
//   - UserDefinedBundle owns ShaderProgram instances via shared_ptr (in m_programs cache)
//   - GetProgram returns raw pointer (caller does NOT take ownership)
//   - GetPrograms returns vector of raw pointers (caller does NOT take ownership)
//   - Bundle lifetime managed by ShaderBundleManager
//
// Directory Structure:
//   shaders/bundle/{bundleName}/
//     gbuffers_basic.vs.hlsl
//     gbuffers_basic.ps.hlsl
//     gbuffers_textured.vs.hlsl
//     gbuffers_textured.ps.hlsl
//     ...
//
// Usage:
//   auto bundle = std::make_unique<UserDefinedBundle>("custom_pack", bundlePath);
//   bundle->PrecompileAll();  // Compile all programs
//
//   ShaderProgram* basic = bundle->GetProgram("gbuffers_basic");
//   if (basic) {
//       // Use program
//   }
//
//   auto gbuffers = bundle->GetPrograms("gbuffers_.*");  // Regex match
//   for (ShaderProgram* prog : gbuffers) {
//       // Process matched programs
//   }
//
// Teaching Points:
//   - Precompilation strategy: All shaders compiled at load time, not on-demand
//   - Resilient design: Individual failures don't break entire bundle
//   - Cache pattern: shared_ptr in cache, raw pointer for access (no ownership transfer)
//   - Regex support: Flexible program matching for batch operations
//
//-----------------------------------------------------------------------------------------------

#include <string>
#include <unordered_map>
#include <memory>
#include <filesystem>
#include <vector>

namespace enigma::graphic
{
    // Forward declaration to avoid circular dependencies
    class ShaderProgram;

    //-------------------------------------------------------------------------------------------
    // UserDefinedBundle
    //
    // [NEW] Manages shader programs from a user-defined bundle directory
    //
    // Lifecycle:
    //   1. Construct with bundle name and path
    //   2. Call PrecompileAll() to compile all shader programs
    //   3. Query programs via GetProgram() or GetPrograms()
    //   4. Bundle destruction releases all compiled programs
    //-------------------------------------------------------------------------------------------
    class UserDefinedBundle
    {
    public:
        //-------------------------------------------------------------------------------------------
        // Constructor
        //
        // [NEW] Initialize bundle with name and path
        //
        // Parameters:
        //   bundleName - Display name of the bundle (e.g., "custom_pack")
        //   bundlePath - Full path to bundle directory (e.g., "shaders/bundle/custom_pack/")
        //
        // Note: Does NOT compile shaders. Call PrecompileAll() after construction.
        //-------------------------------------------------------------------------------------------
        UserDefinedBundle(const std::string&           bundleName,
                          const std::filesystem::path& bundlePath);

        //-------------------------------------------------------------------------------------------
        // Destructor
        //
        // [DEFAULT] Releases all compiled programs via shared_ptr automatic cleanup
        //-------------------------------------------------------------------------------------------
        ~UserDefinedBundle() = default;

        // Non-copyable, movable
        UserDefinedBundle(const UserDefinedBundle&)                = delete;
        UserDefinedBundle& operator=(const UserDefinedBundle&)     = delete;
        UserDefinedBundle(UserDefinedBundle&&) noexcept            = default;
        UserDefinedBundle& operator=(UserDefinedBundle&&) noexcept = default;

        //-------------------------------------------------------------------------------------------
        // PrecompileAll
        //
        // [NEW] Compile all shader programs in the bundle directory
        //
        // Workflow:
        //   1. Scan directory for valid VS/PS shader pairs using ShaderScanHelper
        //   2. For each program:
        //      a. Find shader files (VS and PS)
        //      b. Call RendererSubsystem::CreateShaderProgramFromFiles()
        //      c. Store successful programs in cache
        //      d. Log warnings for failed compilations (continue with others)
        //
        // Error Handling:
        //   - Individual compilation failures are logged as warnings
        //   - Does NOT throw exceptions or fail entire load
        //   - Resilient design: Load as many programs as possible
        //
        // Precondition: g_theRendererSubsystem must be initialized
        //-------------------------------------------------------------------------------------------
        void PrecompileAll();

        //-------------------------------------------------------------------------------------------
        // GetProgram
        //
        // [NEW] Get a single shader program by exact name
        //
        // Parameters:
        //   programName - Exact program name (e.g., "gbuffers_basic")
        //
        // Returns:
        //   Raw pointer to ShaderProgram if found, nullptr otherwise
        //   Caller does NOT take ownership (program remains in cache)
        //
        // Note: Does NOT throw exception if not found
        //-------------------------------------------------------------------------------------------
        std::shared_ptr<ShaderProgram> GetProgram(const std::string& programName);

        //-------------------------------------------------------------------------------------------
        // GetPrograms
        //
        // [NEW] Get multiple shader programs matching a regex pattern
        //
        // Parameters:
        //   searchRule - Regex pattern (e.g., "gbuffers_.*" matches gbuffers_basic, gbuffers_textured)
        //
        // Returns:
        //   Vector of raw pointers to matching programs
        //   Empty vector if no matches (does NOT throw)
        //   Caller does NOT take ownership
        //-------------------------------------------------------------------------------------------
        std::vector<std::shared_ptr<ShaderProgram>> GetPrograms(const std::string& searchRule);

        //-------------------------------------------------------------------------------------------
        // HasProgram
        //
        // [NEW] Check if a program exists in the cache
        //
        // Parameters:
        //   programName - Exact program name to check
        //
        // Returns:
        //   true if program exists in cache, false otherwise
        //-------------------------------------------------------------------------------------------
        bool HasProgram(const std::string& programName) const;

        //-------------------------------------------------------------------------------------------
        // GetName
        //
        // [NEW] Get the bundle name
        //
        // Returns:
        //   Const reference to bundle name string
        //-------------------------------------------------------------------------------------------
        const std::string& GetName() const { return m_bundleName; }

        //-------------------------------------------------------------------------------------------
        // GetPath
        //
        // [NEW] Get the bundle directory path
        //
        // Returns:
        //   Const reference to bundle path
        //-------------------------------------------------------------------------------------------
        const std::filesystem::path& GetPath() const { return m_bundlePath; }

        //-------------------------------------------------------------------------------------------
        // GetProgramCount
        //
        // [NEW] Get the number of successfully compiled programs
        //
        // Returns:
        //   Number of programs in cache
        //-------------------------------------------------------------------------------------------
        size_t GetProgramCount() const { return m_programs.size(); }

    private:
        //-------------------------------------------------------------------------------------------
        // Member Variables
        //-------------------------------------------------------------------------------------------
        std::string           m_bundleName; // Display name of the bundle
        std::filesystem::path m_bundlePath; // Full path to bundle directory

        // Program cache: programName -> shared_ptr<ShaderProgram>
        // shared_ptr ensures automatic cleanup when bundle is destroyed
        std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> m_programs;
    };
} // namespace enigma::graphic
