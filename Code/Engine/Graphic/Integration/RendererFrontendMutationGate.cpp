#include "Engine/Graphic/Integration/RendererFrontendMutationGate.hpp"

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/StringUtils.hpp"

namespace enigma::graphic
{
    bool RendererFrontendMutationGate::TryBegin(RendererFrontendMutationOwner owner, const char* reason)
    {
        if (owner == RendererFrontendMutationOwner::None)
        {
            ERROR_AND_DIE("RendererFrontendMutationGate::TryBegin called with None owner")
            return false;
        }

        const std::string safeReason = reason != nullptr ? reason : "";
        if (m_snapshot.IsBusy())
        {
            m_snapshot.lastBlockedOwner = owner;
            m_snapshot.lastBlockedReason = safeReason;
            ++m_snapshot.blockedBeginCount;
            return false;
        }

        m_snapshot.owner = owner;
        m_snapshot.reason = safeReason;
        ++m_snapshot.generation;
        return true;
    }

    void RendererFrontendMutationGate::End(RendererFrontendMutationOwner owner)
    {
        if (!m_snapshot.IsBusy())
        {
            ERROR_AND_DIE(Stringf(
                "RendererFrontendMutationGate::End called while idle. requestedOwner=%s",
                ToString(owner)))
            return;
        }

        if (m_snapshot.owner != owner)
        {
            ERROR_AND_DIE(Stringf(
                "RendererFrontendMutationGate::End owner mismatch. activeOwner=%s requestedOwner=%s reason=%s",
                ToString(m_snapshot.owner),
                ToString(owner),
                m_snapshot.reason.c_str()))
            return;
        }

        m_snapshot.owner = RendererFrontendMutationOwner::None;
        m_snapshot.reason.clear();
    }

    bool RendererFrontendMutationGate::IsBusy() const noexcept
    {
        return m_snapshot.IsBusy();
    }

    RendererFrontendMutationOwner RendererFrontendMutationGate::GetOwner() const noexcept
    {
        return m_snapshot.owner;
    }

    RendererFrontendMutationGateSnapshot RendererFrontendMutationGate::GetDiagnosticsSnapshot() const
    {
        return m_snapshot;
    }

    const char* ToString(RendererFrontendMutationOwner owner) noexcept
    {
        switch (owner)
        {
        case RendererFrontendMutationOwner::None:
            return "None";
        case RendererFrontendMutationOwner::ShaderBundleReload:
            return "ShaderBundleReload";
        case RendererFrontendMutationOwner::WindowResize:
            return "WindowResize";
        }

        return "Unknown";
    }

    RendererFrontendMutationGate& GetRendererFrontendMutationGate() noexcept
    {
        static RendererFrontendMutationGate gate;
        return gate;
    }
} // namespace enigma::graphic
