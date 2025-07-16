#include "GlbModelLoader.hpp"

#include "Engine/Core/EngineCommon.hpp"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "ThirdParty/tinygltf/tiny_gltf.h"

GlbModelLoader::GlbModelLoader(IRenderer* renderer) : ModelLoader(renderer), m_loader(std::make_unique<tinygltf::TinyGLTF>())
{
}

GlbModelLoader::~GlbModelLoader() = default;

bool GlbModelLoader::CanLoad(const std::string& extension) const
{
    UNUSED(extension)
    // TODO: Check whether or not have meta file and extension matches
    return true;
}

std::unique_ptr<FMesh> GlbModelLoader::Load(const ResourceLocation& location, const std::string& filePath)
{
    tinygltf::Model model;
    std::string     err;
    std::string     warn;

    bool success = false;


    std::unique_ptr<FMesh> mesh;

    return mesh;
}

std::unique_ptr<FMesh> GlbModelLoader::ProcessMesh(const tinygltf::Model& model, int meshIndex)
{
    return nullptr;
}

void GlbModelLoader::ProcessPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, FMesh& mesh)
{
}

void GlbModelLoader::ExtractPositions(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
}

void GlbModelLoader::ExtractNormals(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
}

void GlbModelLoader::ExtractTexCoords(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
}

void GlbModelLoader::ExtractTangents(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
}

void GlbModelLoader::ExtractIndices(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
}

void GlbModelLoader::ExtractMaterials(const tinygltf::Model& model, FMesh& mesh)
{
    mesh.materials.reserve(model.materials.size());

    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const auto& gltfMaterial = model.materials[i];
        FMaterial   material;
        material.name = gltfMaterial.name.empty() ? ("Material_" + std::to_string(i)) : gltfMaterial.name;

        ProcessMaterial(gltfMaterial, material, model);
        mesh.materials.push_back(std::move(material));

        //LOG(LogResource, Info, Stringf("Extracted material: %s", material.name.c_str()).c_str());
    }

    // 如果没有材质，创建默认材质
    if (mesh.materials.empty())
    {
        FMaterial defaultMaterial;
        defaultMaterial.name = "DefaultMaterial";
        mesh.materials.push_back(std::move(defaultMaterial));
    }
}

void GlbModelLoader::ProcessMaterial(const tinygltf::Material& gltfMaterial, FMaterial& material, const tinygltf::Model& model)
{
    const auto& pbr = gltfMaterial.pbrMetallicRoughness;
    // Extract PBP parameter
    if (pbr.baseColorFactor.size() >= 4)
    {
        material.baseColorFactor = Vec4(
            static_cast<float>(pbr.baseColorFactor[0]),
            static_cast<float>(pbr.baseColorFactor[1]),
            static_cast<float>(pbr.baseColorFactor[2]),
            static_cast<float>(pbr.baseColorFactor[3])
        );
    }

    material.metallicFactor  = static_cast<float>(pbr.metallicFactor);
    material.roughnessFactor = static_cast<float>(pbr.roughnessFactor);

    if (gltfMaterial.emissiveFactor.size() >= 3)
    {
        material.emissiveFactor = Vec3(
            static_cast<float>(gltfMaterial.emissiveFactor[0]),
            static_cast<float>(gltfMaterial.emissiveFactor[1]),
            static_cast<float>(gltfMaterial.emissiveFactor[2])
        );
    }

    // Set Blend mode
    if (gltfMaterial.alphaMode == "MASK")
    {
        material.alphaMode   = FMaterial::AlphaMode::MASK;
        material.alphaCutoff = static_cast<float>(gltfMaterial.alphaCutoff);
    }
    else if (gltfMaterial.alphaMode == "BLEND")
    {
        material.alphaMode = FMaterial::AlphaMode::BLEND;
    }

    material.doubleSided = gltfMaterial.doubleSided;

    // Texture Extraction
    // 1. Albedo纹理
    if (pbr.baseColorTexture.index >= 0)
    {
        auto albedoTexture = ExtractTextureFromInfo(pbr.baseColorTexture, model, "albedo");
        if (albedoTexture)
        {
            material.textureCoordSets[EMaterialChannel::Albedo] = pbr.baseColorTexture.texCoord;
            material.SetTexture(EMaterialChannel::Albedo, std::move(albedoTexture));
        }
    }

    // 2. Metallic-Roughness纹理
    if (pbr.metallicRoughnessTexture.index >= 0)
    {
        auto metallicTexture = ExtractTextureFromInfo(pbr.metallicRoughnessTexture, model, "metallic_roughness");
        if (metallicTexture)
        {
            material.textureCoordSets[EMaterialChannel::MetallicRoughness] = pbr.metallicRoughnessTexture.texCoord;
            material.SetTexture(EMaterialChannel::MetallicRoughness, std::move(metallicTexture));
            //LOG(LogResource, Info, "  -> MetallicRoughness texture extracted");
        }
    }

    // 3. Normal纹理
    if (gltfMaterial.normalTexture.index >= 0)
    {
        auto normalTexture = ExtractTextureFromNormalInfo(gltfMaterial.normalTexture, model);
        if (normalTexture)
        {
            material.normalScale                                = static_cast<float>(gltfMaterial.normalTexture.scale);
            material.textureCoordSets[EMaterialChannel::Normal] = gltfMaterial.normalTexture.texCoord;
            material.SetTexture(EMaterialChannel::Normal, std::move(normalTexture));
            //LOG(LogResource, Info, "  -> Normal texture extracted");
        }
    }

    // 4. AO纹理
    if (gltfMaterial.occlusionTexture.index >= 0)
    {
        auto aoTexture = ExtractTextureFromOcclusionInfo(gltfMaterial.occlusionTexture, model);
        if (aoTexture)
        {
            material.occlusionStrength                             = static_cast<float>(gltfMaterial.occlusionTexture.strength);
            material.textureCoordSets[EMaterialChannel::Occlusion] = gltfMaterial.occlusionTexture.texCoord;
            material.SetTexture(EMaterialChannel::Occlusion, std::move(aoTexture));
            //LOG(LogResource, Info, "  -> AO texture extracted");
        }
    }

    // 5. Emission纹理
    if (gltfMaterial.emissiveTexture.index >= 0)
    {
        auto emissionTexture = ExtractTextureFromInfo(gltfMaterial.emissiveTexture, model, "emission");
        if (emissionTexture)
        {
            material.textureCoordSets[EMaterialChannel::Emission] = gltfMaterial.emissiveTexture.texCoord;
            material.SetTexture(EMaterialChannel::Emission, std::move(emissionTexture));
            // /LOG(LogResource, Info, "  -> Emission texture extracted");
        }
    }
}

std::unique_ptr<Texture> GlbModelLoader::CreateTextureFromGLTFImage(const tinygltf::Image& image, const std::string& debugName)
{
}

std::unique_ptr<Texture> GlbModelLoader::ExtractTextureFromInfo(const tinygltf::TextureInfo& textureInfo, const tinygltf::Model& model, const std::string& channelName)
{
}

std::unique_ptr<Texture> GlbModelLoader::ExtractTextureFromNormalInfo(const tinygltf::NormalTextureInfo& normalInfo, const tinygltf::Model& model)
{
}

std::unique_ptr<Texture> GlbModelLoader::ExtractTextureFromOcclusionInfo(const tinygltf::OcclusionTextureInfo& occlusionInfo, const tinygltf::Model& model)
{
}
