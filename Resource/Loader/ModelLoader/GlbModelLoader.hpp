#pragma once
#include "ModelLoader.hpp"

class GlbModelLoader : public ModelLoader
{
public:
    GlbModelLoader(IRenderer* renderer);

    bool                   CanLoad(const std::string& extension) const override;
    std::unique_ptr<FMesh> Load(const ResourceLocation& location, const std::string& filePath) override;
    int                    GetPriority() const override { return 101; }
    std::string            GetLoaderName() const override { return "GlbModelLoader"; }

private:
};
