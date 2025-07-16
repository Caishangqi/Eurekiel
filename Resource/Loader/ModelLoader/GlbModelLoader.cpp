#include "GlbModelLoader.hpp"

#include "Engine/Core/EngineCommon.hpp"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "Engine/Renderer/Texture.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Renderer/IRenderer.hpp"
#include "ThirdParty/tinygltf/tiny_gltf.h"

GlbModelLoader::GlbModelLoader(IRenderer* renderer) : ModelLoader(renderer), m_loader(std::make_unique<tinygltf::TinyGLTF>())
{
}

GlbModelLoader::~GlbModelLoader() = default;

bool GlbModelLoader::CanLoad(const std::string& extension) const
{
    return extension == ".glb" || extension == ".gltf";
}

std::unique_ptr<FMesh> GlbModelLoader::Load(const ResourceLocation& location, const std::string& filePath)
{
    tinygltf::Model model;
    std::string     err;
    std::string     warn;

    bool success = false;

    std::filesystem::path path = filePath;

    if (path.extension() == ".glb")
    {
        success = m_loader->LoadBinaryFromFile(&model, &err, &warn, filePath);
    }
    else if (path.extension() == ".gltf")
    {
        success = m_loader->LoadASCIIFromFile(&model, &err, &warn, filePath);
    }
    else
    {
        ERROR_AND_DIE(Stringf("Unsupported file format: %s", filePath.c_str()));
    }

    if (!success)
    {
        ERROR_AND_DIE(Stringf("Failed to load GLTF file %s: %s", filePath.c_str(), err.c_str()));
    }

    auto mesh = std::make_unique<FMesh>();

    // 处理所有mesh中的几何数据
    for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
    {
        const auto& gltfMesh = model.meshes[meshIndex];

        for (const auto& primitive : gltfMesh.primitives)
        {
            ProcessPrimitive(model, primitive, *mesh);
        }
    }

    // 提取材质和纹理通道
    ExtractMaterials(model, *mesh);

    return mesh;
}

std::unique_ptr<FMesh> GlbModelLoader::ProcessMesh(const tinygltf::Model& model, int meshIndex)
{
    return nullptr;
}

void GlbModelLoader::ProcessPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, FMesh& mesh)
{
    size_t vertexOffset = mesh.m_vertices.size();

    // 提取顶点属性
    auto positionIter = primitive.attributes.find("POSITION");
    if (positionIter != primitive.attributes.end())
    {
        ExtractPositions(model, positionIter->second, mesh);
    }

    auto normalIter = primitive.attributes.find("NORMAL");
    if (normalIter != primitive.attributes.end())
    {
        ExtractNormals(model, normalIter->second, mesh);
    }

    auto texCoordIter = primitive.attributes.find("TEXCOORD_0");
    if (texCoordIter != primitive.attributes.end())
    {
        ExtractTexCoords(model, texCoordIter->second, mesh);
    }

    auto tangentIter = primitive.attributes.find("TANGENT");
    if (tangentIter != primitive.attributes.end())
    {
        ExtractTangents(model, tangentIter->second, mesh);
    }

    // 提取索引
    if (primitive.indices >= 0)
    {
        size_t indexOffset = mesh.m_indices.size();
        ExtractIndices(model, primitive.indices, mesh);

        // 调整索引偏移
        for (size_t i = indexOffset; i < mesh.m_indices.size(); ++i)
        {
            mesh.m_indices[i] += static_cast<unsigned int>(vertexOffset);
        }
    }
}

void GlbModelLoader::ExtractPositions(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
    const auto& accessor   = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer     = model.buffers[bufferView.buffer];

    GUARANTEE_OR_DIE(accessor.type == TINYGLTF_TYPE_VEC3, "Position accessor must be VEC3");
    GUARANTEE_OR_DIE(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "Position must be float");

    const float* positions = reinterpret_cast<const float*>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset]
    );

    size_t vertexCount = accessor.count;
    size_t currentSize = mesh.m_vertices.size();
    mesh.m_vertices.resize(currentSize + vertexCount);

    for (size_t i = 0; i < vertexCount; ++i)
    {
        Vec3 pos(positions[i * 3], positions[i * 3 + 1], positions[i * 3 + 2]);
        mesh.m_vertices[currentSize + i].m_position = pos;
    }
}

void GlbModelLoader::ExtractNormals(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
    const auto& accessor   = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer     = model.buffers[bufferView.buffer];

    GUARANTEE_OR_DIE(accessor.type == TINYGLTF_TYPE_VEC3, "Normal accessor must be VEC3")
    GUARANTEE_OR_DIE(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "Normal must be float")

    const float* normals = reinterpret_cast<const float*>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset]
    );

    size_t vertexCount = accessor.count;
    size_t currentSize = mesh.m_vertices.size() - vertexCount; // 假设positions已经添加

    for (size_t i = 0; i < vertexCount; ++i)
    {
        Vec3 normal(normals[i * 3], normals[i * 3 + 1], normals[i * 3 + 2]);
        mesh.m_vertices[currentSize + i].m_normal = normal;
    }
}

void GlbModelLoader::ExtractTexCoords(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
    const auto& accessor   = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer     = model.buffers[bufferView.buffer];

    GUARANTEE_OR_DIE(accessor.type == TINYGLTF_TYPE_VEC2, "TexCoord accessor must be VEC2");
    GUARANTEE_OR_DIE(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "TexCoord must be float");

    const float* texCoords = reinterpret_cast<const float*>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset]
    );

    size_t vertexCount = accessor.count;
    size_t currentSize = mesh.m_vertices.size() - vertexCount;

    for (size_t i = 0; i < vertexCount; ++i)
    {
        Vec2 uv(texCoords[i * 2], texCoords[i * 2 + 1]);
        mesh.m_vertices[currentSize + i].m_uvTexCoords = uv;
    }
}

void GlbModelLoader::ExtractTangents(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
    const auto& accessor   = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer     = model.buffers[bufferView.buffer];

    GUARANTEE_OR_DIE(accessor.type == TINYGLTF_TYPE_VEC4, "Tangent accessor must be VEC4");
    GUARANTEE_OR_DIE(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "Tangent must be float");

    const float* tangents = reinterpret_cast<const float*>(
        &buffer.data[bufferView.byteOffset + accessor.byteOffset]
    );

    size_t vertexCount = accessor.count;
    size_t currentSize = mesh.m_vertices.size() - vertexCount;

    for (size_t i = 0; i < vertexCount; ++i)
    {
        Vec3 tangent(tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2]);
        mesh.m_vertices[currentSize + i].m_tangent = tangent;
        // 第四个分量（w）通常用于计算bitangent的方向，这里暂时忽略
    }
}

void GlbModelLoader::ExtractIndices(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
    const auto& accessor   = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer     = model.buffers[bufferView.buffer];

    size_t indexCount  = accessor.count;
    size_t currentSize = mesh.m_indices.size();
    mesh.m_indices.resize(currentSize + indexCount);

    const unsigned char* data = &buffer.data[bufferView.byteOffset + accessor.byteOffset];

    switch (accessor.componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        {
            for (size_t i = 0; i < indexCount; ++i)
            {
                mesh.m_indices[currentSize + i] = static_cast<unsigned int>(data[i]);
            }
            break;
        }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
            const unsigned short* indices = reinterpret_cast<const unsigned short*>(data);
            for (size_t i = 0; i < indexCount; ++i)
            {
                mesh.m_indices[currentSize + i] = static_cast<unsigned int>(indices[i]);
            }
            break;
        }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        {
            const unsigned int* indices = reinterpret_cast<const unsigned int*>(data);
            for (size_t i = 0; i < indexCount; ++i)
            {
                mesh.m_indices[currentSize + i] = indices[i];
            }
            break;
        }
    default:
        ERROR_AND_DIE("Unsupported index component type");
    }
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
        if (pbr.metallicRoughnessTexture.index >= 0)
        {
            auto metallicTexture = ExtractTextureFromInfo(pbr.metallicRoughnessTexture, model, "metallic_roughness");
            if (metallicTexture)
            {
                material.textureCoordSets[EMaterialChannel::MetallicRoughness] = pbr.metallicRoughnessTexture.texCoord;
                material.SetTexture(EMaterialChannel::MetallicRoughness, std::move(metallicTexture));
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
        }
    }
}

std::unique_ptr<Texture> GlbModelLoader::CreateTextureFromGLTFImage(const tinygltf::Image& gltfImage, const std::string& debugName)
{
    if (gltfImage.image.empty())
    {
        return nullptr;
    }

    // 创建引擎的Image对象
    IntVec2 dimensions(gltfImage.width, gltfImage.height);
    Image   engineImage(dimensions, Rgba8::WHITE); // 创建具有正确尺寸的Image

    // 手动设置像素数据
    const unsigned char* srcData     = gltfImage.image.data();
    int                  totalPixels = gltfImage.width * gltfImage.height;

    // 根据通道数转换像素格式
    if (gltfImage.component == 1)
    {
        // 灰度图
        for (int i = 0; i < totalPixels; ++i)
        {
            unsigned char gray = srcData[i];
            int           x    = i % gltfImage.width;
            int           y    = i / gltfImage.width;
            engineImage.SetTexelColor(IntVec2(x, y), Rgba8(gray, gray, gray, 255));
        }
    }
    else if (gltfImage.component == 3)
    {
        // RGB
        for (int i = 0; i < totalPixels; ++i)
        {
            int x = i % gltfImage.width;
            int y = i / gltfImage.width;
            engineImage.SetTexelColor(IntVec2(x, y),
                                      Rgba8(srcData[i * 3 + 0], // R
                                            srcData[i * 3 + 1], // G
                                            srcData[i * 3 + 2], // B
                                            255)); // A
        }
    }
    else if (gltfImage.component == 4)
    {
        // RGBA
        for (int i = 0; i < totalPixels; ++i)
        {
            int x = i % gltfImage.width;
            int y = i / gltfImage.width;
            engineImage.SetTexelColor(IntVec2(x, y),
                                      Rgba8(srcData[i * 4 + 0], // R
                                            srcData[i * 4 + 1], // G
                                            srcData[i * 4 + 2], // B
                                            srcData[i * 4 + 3])); // A
        }
    }
    else
    {
        return nullptr;
    }

    // 使用现有的IRenderer接口创建纹理，并立即包装为智能指针
    Texture* rawTexture = m_renderer->CreateTextureFromImage(engineImage);
    if (rawTexture)
    {
        // 立即包装为智能指针，确保RAII和所有权转移
        return std::unique_ptr<Texture>{rawTexture};
    }

    return nullptr;
}

std::unique_ptr<Texture> GlbModelLoader::ExtractTextureFromInfo(const tinygltf::TextureInfo& textureInfo, const tinygltf::Model& model, const std::string& channelName)
{
    if (textureInfo.index < 0 || textureInfo.index >= model.textures.size())
    {
        return nullptr;
    }

    const auto& texture = model.textures[textureInfo.index];
    if (texture.source < 0 || texture.source >= model.images.size())
    {
        return nullptr;
    }

    const auto& image     = model.images[texture.source];
    std::string debugName = channelName + "_" + std::to_string(textureInfo.index);

    return CreateTextureFromGLTFImage(image, debugName);
}

std::unique_ptr<Texture> GlbModelLoader::ExtractTextureFromNormalInfo(const tinygltf::NormalTextureInfo& normalInfo, const tinygltf::Model& model)
{
    tinygltf::TextureInfo textureInfo;
    textureInfo.index    = normalInfo.index;
    textureInfo.texCoord = normalInfo.texCoord;
    return ExtractTextureFromInfo(textureInfo, model, "normal");
}

std::unique_ptr<Texture> GlbModelLoader::ExtractTextureFromOcclusionInfo(const tinygltf::OcclusionTextureInfo& occlusionInfo, const tinygltf::Model& model)
{
    tinygltf::TextureInfo textureInfo;
    textureInfo.index    = occlusionInfo.index;
    textureInfo.texCoord = occlusionInfo.texCoord;
    return ExtractTextureFromInfo(textureInfo, model, "ao");
}
