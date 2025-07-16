#pragma once
#include "ModelLoader.hpp"

namespace tinygltf
{
    struct OcclusionTextureInfo;
    struct NormalTextureInfo;
    struct TextureInfo;
    struct Image;
    struct Material;
    class Model;
    class TinyGLTF;
    class Mesh;
    class Primitive;
}

class GlbModelLoader : public ModelLoader
{
public:
    GlbModelLoader(IRenderer* renderer);
    ~GlbModelLoader() override;

    bool                   CanLoad(const std::string& extension) const override;
    std::unique_ptr<FMesh> Load(const ResourceLocation& location, const std::string& filePath) override;
    int                    GetPriority() const override { return 101; }
    std::string            GetLoaderName() const override { return "GlbModelLoader"; }

private:
    std::unique_ptr<tinygltf::TinyGLTF> m_loader;

    // Helper functions
    std::unique_ptr<FMesh> ProcessMesh(const tinygltf::Model& model, int meshIndex);
    void                   ProcessPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, FMesh& mesh);

    // Data generation
    void CalculateTangentsAndBitangents(std::unique_ptr<FMesh>::element_type& mesh);

    // Data extraction
    void ExtractPositions(const tinygltf::Model& model, int accessorIndex, FMesh& mesh);
    void ExtractNormals(const tinygltf::Model& model, int accessorIndex, FMesh& mesh);
    void ExtractTexCoords(const tinygltf::Model& model, int accessorIndex, FMesh& mesh);
    void ExtractTangents(const tinygltf::Model& model, int accessorIndex, FMesh& mesh);
    void ExtractIndices(const tinygltf::Model& model, int accessorIndex, FMesh& mesh);

    // Material extraction
    void ExtractMaterials(const tinygltf::Model& model, FMesh& mesh);
    void ProcessMaterial(const tinygltf::Material& gltfMaterial, FMaterial& material, const tinygltf::Model& model);

    // Create texture from image from tinygltf::Image
    std::unique_ptr<Texture> CreateTextureFromGLTFImage(const tinygltf::Image& gltfImage, const std::string& debugName);
    std::unique_ptr<Texture> ExtractTextureFromInfo(const tinygltf::TextureInfo& textureInfo, const tinygltf::Model& model, const std::string& channelName);
    std::unique_ptr<Texture> ExtractTextureFromNormalInfo(const tinygltf::NormalTextureInfo& normalInfo, const tinygltf::Model& model);
    std::unique_ptr<Texture> ExtractTextureFromOcclusionInfo(const tinygltf::OcclusionTextureInfo& occlusionInfo, const tinygltf::Model& model);

    // Accessor
    template <typename T>
    void ExtractAccessorData(const tinygltf::Model& model, int accessorIndex, std::vector<T>& output);
};

template <typename T>
void GlbModelLoader::ExtractAccessorData(const tinygltf::Model& model, int accessorIndex, std::vector<T>& output)
{
}
