#pragma once
//-----------------------------------------------------------------------------------------------
// ShaderBundle.hpp
//
// [NEW] Complete shader bundle management with three-tier fallback mechanism
//
// This class provides:
//   - Management of multiple UserDefinedBundles (from bundle/ subdirectories)
//   - Three-tier fallback: Current UserBundle -> Program folder -> Engine bundle
//   - GetProgram variants for flexible querying
//   - SwitchBundle for runtime bundle switching
//   - ProgramFallbackChain integration
//
// Design Principles (SOLID + KISS):
//   - Single Responsibility: Manages shader bundle resources and fallback logic
//   - Open/Closed: Extensible via UserDefinedBundle without modifying this class
//   - Dependency Inversion: Depends on abstractions (ShaderProgram pointer)
//   - Resilient Design: Fallback mechanism ensures graceful degradation
//
// Ownership Model:
//   - ShaderBundle owns UserDefinedBundles via unique_ptr (exclusive)
//   - ShaderBundle owns ProgramFallbackChain via unique_ptr (exclusive)
//   - ShaderBundle owns program/ folder cache via shared_ptr (can be shared)
//   - Engine bundle reference via shared_ptr (injected, shared ownership)
//   - GetProgram returns raw pointer (NO ownership transfer)
//
// Three-Tier Fallback Mechanism:
//   Level 1: Current UserDefinedBundle (bundle/{current}/)
//   Level 2: Program folder with fallback rules (program/)
//   Level 3: Engine bundle (if this is user bundle)
//
// Directory Structure:
//   {bundlePath}/
//     shaders/
//       bundle/
//         mycustom_bundle_0/
//           gbuffers_basic.vs.hlsl
//           gbuffers_basic.ps.hlsl
//         mycustom_bundle_1/
//           ...
//       program/
//         gbuffers_basic.vs.hlsl
//         gbuffers_basic.ps.hlsl
//         final.vs.hlsl
//         final.ps.hlsl
//       fallback_rule.json
//
// Usage:
//   auto bundle = std::make_shared<ShaderBundle>(meta, engineBundle);
//   bundle->Initialize();
//
//   // Query shader program with three-tier fallback
//   ShaderProgram* basic = bundle->GetProgram("gbuffers_basic");
//   if (basic) {
//       // Use program
//   }
//
//   // Switch to different UserDefinedBundle
//   if (bundle->SwitchBundle("mycustom_bundle_1")) {
//       // Now using mycustom_bundle_1 as primary
//   }
//
// Teaching Points:
//   - Three-tier fallback ensures shader availability
//   - UserDefinedBundle provides bundle-specific shader customization
//   - ProgramFallbackChain handles graceful degradation between shaders
//   - Engine bundle provides ultimate fallback for user bundles
//
//-----------------------------------------------------------------------------------------------

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <filesystem>

#include "Engine/Graphic/Bundle/ShaderBundleCommon.hpp"
#include "Engine/Graphic/Bundle/UserDefinedBundle.hpp"
#include "Engine/Graphic/Bundle/ProgramFallbackChain.hpp"
#include "Engine/Graphic/Bundle/Directive/PackRenderTargetDirectives.hpp"
#include "Engine/Graphic/Shader/Program/Parsing/ConstDirectiveParser.hpp"

namespace enigma::graphic
{
    // Forward declaration to avoid circular dependencies
    class ShaderProgram;

    //-------------------------------------------------------------------------------------------
    // ShaderBundle
    //
    // [NEW] Complete shader bundle with three-tier fallback mechanism
    //
    // Lifecycle:
    //   1. Construct with metadata and optional engine bundle reference
    //   2. Call Initialize() to load fallback rules and discover UserDefinedBundles
    //   3. Query programs via GetProgram() or GetPrograms()
    //   4. Optionally switch between UserDefinedBundles via SwitchBundle()
    //   5. Bundle destruction releases all compiled programs
    //-------------------------------------------------------------------------------------------
    class ShaderBundle
    {
    public:
        //-------------------------------------------------------------------------------------------
        // Constructor (RAII)
        //
        // [REFACTOR] Initialize bundle with metadata and optional engine bundle reference
        //
        // Parameters:
        //   meta - ShaderBundleMeta containing name, path, and isEngineBundle flag
        //   engineBundle - Reference to engine bundle (nullptr for engine bundle itself)
        //   pathAliases - [NEW] Optional path aliases for include resolution (e.g., @engine -> path)
        //
        // RAII Workflow:
        //   1. Load fallback rules from shaders/fallback_rule.json via ProgramFallbackChain
        //   2. Scan shaders/bundle/ directory for UserDefinedBundle subdirectories
        //   3. Create and precompile all UserDefinedBundles
        //   4. Set first UserDefinedBundle as current (if any exist)
        //   5. [NEW] Parse RT directives with alias-aware include expansion
        //
        // Error Handling:
        //   - Missing fallback_rule.json: fallback disabled (not an error)
        //   - Individual compilation failures in UserDefinedBundle: logged as warnings
        //   - Empty bundle/ directory: valid state (only program/ folder used)
        //
        // Precondition: g_theRendererSubsystem must be initialized
        //-------------------------------------------------------------------------------------------
        explicit ShaderBundle(const ShaderBundleMeta&                             meta,
                              std::shared_ptr<ShaderBundle>                       engineBundle = nullptr,
                              const std::unordered_map<std::string, std::string>& pathAliases  = {});

        //-------------------------------------------------------------------------------------------
        // Destructor
        //
        // [DEFAULT] Releases all owned resources via smart pointer automatic cleanup
        //-------------------------------------------------------------------------------------------
        ~ShaderBundle() = default;

        // Non-copyable, movable
        ShaderBundle(const ShaderBundle&)                = delete;
        ShaderBundle& operator=(const ShaderBundle&)     = delete;
        ShaderBundle(ShaderBundle&&) noexcept            = default;
        ShaderBundle& operator=(ShaderBundle&&) noexcept = default;

        //-------------------------------------------------------------------------------------------
        // GetProgram (single program, default bundle)
        //
        // [NEW] Get a shader program by name with three-tier fallback
        //
        // Parameters:
        //   programName - Program name (e.g., "gbuffers_basic")
        //   enableFallback - If true, apply fallback chain when not found directly
        //
        // Returns:
        //   Raw pointer to ShaderProgram if found, nullptr otherwise
        //   Caller does NOT take ownership
        //
        // Fallback Order:
        //   Level 1: Current UserDefinedBundle (bundle/{current}/)
        //   Level 2: Program folder with fallback rules (program/)
        //   Level 3: Engine bundle (if this is user bundle)
        //
        // Note: Does NOT throw exception if not found at all levels
        //-------------------------------------------------------------------------------------------
        std::shared_ptr<ShaderProgram> GetProgram(const std::string& programName, bool enableFallback = true);

        //-------------------------------------------------------------------------------------------
        // GetProgram (single program, specific bundle)
        //
        // [NEW] Get a shader program from a specific UserDefinedBundle
        //
        // Parameters:
        //   bundleName - Name of the UserDefinedBundle to query
        //   programName - Program name (e.g., "gbuffers_basic")
        //   enableFallback - If true, apply fallback chain when not found in specified bundle
        //
        // Returns:
        //   Raw pointer to ShaderProgram if found, nullptr otherwise
        //   Caller does NOT take ownership
        //
        // Fallback Order (same three-tier, but starts from specified bundle):
        //   Level 1: Specified UserDefinedBundle (bundle/{bundleName}/)
        //   Level 2: Program folder with fallback rules (program/)
        //   Level 3: Engine bundle (if this is user bundle)
        //
        // Note: If bundleName not found, returns nullptr (no exception)
        //-------------------------------------------------------------------------------------------
        std::shared_ptr<ShaderProgram> GetProgram(const std::string& bundleName,
                                                  const std::string& programName,
                                                  bool               enableFallback = true);

        //-------------------------------------------------------------------------------------------
        // GetPrograms (batch query)
        //
        // [NEW] Get multiple shader programs matching a regex pattern
        //
        // Parameters:
        //   searchRule - Regex pattern (e.g., "gbuffers_.*")
        //   enableFallback - If true, include fallback programs
        //
        // Returns:
        //   Vector of raw pointers to matching programs
        //   Empty vector if no matches (does NOT throw)
        //   Caller does NOT take ownership
        //
        // Note: Only searches current UserDefinedBundle and program/ folder
        //-------------------------------------------------------------------------------------------
        std::vector<std::shared_ptr<ShaderProgram>> GetPrograms(const std::string& searchRule,
                                                                bool               enableFallback = true);

        //-------------------------------------------------------------------------------------------
        // SwitchBundle
        //
        // [NEW] Switch the current UserDefinedBundle
        //
        // Parameters:
        //   targetBundleName - Name of the target UserDefinedBundle
        //
        // Returns:
        //   true if switch successful
        //   false if bundle not found OR this is an Engine bundle
        //
        // Behavior:
        //   - Engine bundle: Returns false, logs warning (no exception)
        //   - Non-existent bundle: Returns false, logs warning (no exception)
        //   - Success: Updates m_currentUserBundle pointer, logs info
        //
        // Note: No recompilation, just pointer swap (very fast)
        //-------------------------------------------------------------------------------------------
        bool SwitchBundle(const std::string& targetBundleName);

        //-------------------------------------------------------------------------------------------
        // Fallback Configuration Methods
        //-------------------------------------------------------------------------------------------

        // Returns true if fallback rules were successfully loaded
        bool HasFallbackConfiguration() const;

        // Enable or disable fallback rules, returns new state
        bool EnableFallbackRules(bool newState);

        // Returns current fallback enabled state
        bool GetEnableFallbackRules() const;

        //-------------------------------------------------------------------------------------------
        // Metadata Access Methods
        //-------------------------------------------------------------------------------------------

        // Returns const reference to bundle metadata
        const ShaderBundleMeta& GetMeta() const { return m_meta; }

        // Returns bundle name from metadata
        const std::string& GetName() const { return m_meta.name; }

        // Returns true if this is the engine default bundle
        bool IsEngineBundle() const { return m_meta.isEngineBundle; }

        // Returns count of UserDefinedBundles
        size_t GetUserBundleCount() const { return m_userDefinedBundles.size(); }

        // Returns current UserDefinedBundle name (empty if none)
        std::string GetCurrentUserBundleName() const;

        // Returns list of all UserDefinedBundle names
        std::vector<std::string> GetUserBundleNames() const;

        // [NEW] Returns RT directives parsed from shader sources (for RT format configuration)
        const PackRenderTargetDirectives* GetRTDirectives() const { return m_rtDirectives.get(); }

        //-------------------------------------------------------------------------------------------
        // [NEW] Query const directive values parsed from shader sources
        // Reference: Iris PackDirectives.java - acceptConstFloatDirective, acceptConstIntDirective
        // These are "const float/int/bool" declarations found in shader source code
        //-------------------------------------------------------------------------------------------
        std::optional<float> GetConstFloat(const std::string& name) const { return m_constDirectives.GetFloat(name); }
        std::optional<int>   GetConstInt(const std::string& name) const { return m_constDirectives.GetInt(name); }
        std::optional<bool>  GetConstBool(const std::string& name) const { return m_constDirectives.GetBool(name); }

    private:
        //-------------------------------------------------------------------------------------------
        // Private Helper Methods
        //-------------------------------------------------------------------------------------------

        // Load shader program from program/ folder cache, compile if not cached
        std::shared_ptr<ShaderProgram> LoadFromProgramFolder(const std::string& programName);

        // Find UserDefinedBundle by name, returns nullptr if not found
        UserDefinedBundle* FindUserBundle(const std::string& bundleName);

        //-------------------------------------------------------------------------------------------
        // Member Variables
        //-------------------------------------------------------------------------------------------
        ShaderBundleMeta m_meta; // Bundle metadata (name, path, isEngineBundle)

        // UserDefinedBundles from bundle/ subdirectories (exclusive ownership)
        std::vector<std::unique_ptr<UserDefinedBundle>> m_userDefinedBundles;

        // Current active UserDefinedBundle (raw pointer, owned by m_userDefinedBundles)
        UserDefinedBundle* m_currentUserBundle = nullptr;

        // Fallback chain manager (exclusive ownership)
        std::unique_ptr<ProgramFallbackChain> m_fallbackChain;

        // Program folder cache: programName -> shared_ptr<ShaderProgram>
        // Caches programs loaded from program/ directory (Level 2)
        std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> m_programCache;

        // Reference to engine bundle for Level 3 fallback
        // nullptr if this IS the engine bundle
        std::shared_ptr<ShaderBundle> m_engineBundle;

        // [NEW] RT directives parsed from shader sources (exclusive ownership)
        std::unique_ptr<PackRenderTargetDirectives> m_rtDirectives;

        // [NEW] Const directives parsed from shader sources (sunPathRotation, shadowMapResolution, etc.)
        ConstDirectiveParser m_constDirectives;

        // [NEW] Path aliases for shader include resolution (e.g., @engine -> engine shader path)
        std::unordered_map<std::string, std::string> m_pathAliases;
    };
} // namespace enigma::graphic
