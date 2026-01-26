#pragma once
#include <memory>

#include "Engine/Graphic/Resource/Buffer/D12VertexBuffer.hpp"

namespace enigma::graphic
{
    class FullQuadsRenderer
    {
    public:

    public:
        static void DrawFullQuads();

    public:
        explicit FullQuadsRenderer();
        ~FullQuadsRenderer();
        FullQuadsRenderer(const FullQuadsRenderer&)            = delete;
        FullQuadsRenderer& operator=(const FullQuadsRenderer&) = delete;
        FullQuadsRenderer(FullQuadsRenderer&&)                 = default;
        FullQuadsRenderer& operator=(FullQuadsRenderer&&)      = default;

    private:
        static std::shared_ptr<D12VertexBuffer> m_fullQuadsVertexBuffer;
    };
}
