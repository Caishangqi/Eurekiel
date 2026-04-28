#pragma once

#include <cstdint>
#include <string>

namespace enigma::graphic
{
    enum class RendererFrontendMutationOwner
    {
        None,
        ShaderBundleReload,
        WindowResize
    };

    struct RendererFrontendMutationGateSnapshot
    {
        RendererFrontendMutationOwner owner = RendererFrontendMutationOwner::None;
        std::string                   reason;
        uint64_t                      generation = 0;

        RendererFrontendMutationOwner lastBlockedOwner = RendererFrontendMutationOwner::None;
        std::string                   lastBlockedReason;
        uint64_t                      blockedBeginCount = 0;

        [[nodiscard]] bool IsBusy() const noexcept
        {
            return owner != RendererFrontendMutationOwner::None;
        }
    };

    class RendererFrontendMutationGate
    {
    public:
        [[nodiscard]] bool TryBegin(RendererFrontendMutationOwner owner, const char* reason = nullptr);
        void End(RendererFrontendMutationOwner owner);

        [[nodiscard]] bool IsBusy() const noexcept;
        [[nodiscard]] RendererFrontendMutationOwner GetOwner() const noexcept;
        [[nodiscard]] RendererFrontendMutationGateSnapshot GetDiagnosticsSnapshot() const;

    private:
        RendererFrontendMutationGateSnapshot m_snapshot;
    };

    [[nodiscard]] const char* ToString(RendererFrontendMutationOwner owner) noexcept;
    [[nodiscard]] RendererFrontendMutationGate& GetRendererFrontendMutationGate() noexcept;
} // namespace enigma::graphic
