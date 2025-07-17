#include "GlbModelLoader.hpp"

#include "Engine/Core/EngineCommon.hpp"
#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include "ObjModelLoader.hpp"
#include "Engine/Renderer/Texture.hpp"
#include "Engine/Core/Image.hpp"
#include "Engine/Math/MathUtils.hpp"
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
    UNUSED(location)
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

    // handle geometry data in all meshes
    for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
    {
        const auto& gltfMesh = model.meshes[meshIndex];

        for (const auto& primitive : gltfMesh.primitives)
        {
            ProcessPrimitive(model, primitive, *mesh);
        }
    }

    // Calculate the missing tangents and bitangents
    CalculateTangentsAndBitangents(*mesh);

    // Extract material and texture channels
    ExtractMaterials(model, *mesh);

    return mesh;
}

std::unique_ptr<FMesh> GlbModelLoader::ProcessMesh(const tinygltf::Model& model, int meshIndex)
{
    UNUSED(meshIndex)
    UNUSED(model)
    return nullptr;
}

/**
 * Processes the primitive data of a 3D model, extracting vertex attributes,
 * indices, and other mesh information to populate the provided `mesh` object.
 *
 * @param model The GLTF model containing the primitive data.
 * @param primitive The primitive object within the GLTF model that specifies
 *                  vertex attributes and indices.
 * @param mesh The mesh object to be populated with the extracted data, including
 *             vertices, normals, UV coordinates, tangents, and indices.
 */
void GlbModelLoader::ProcessPrimitive(const tinygltf::Model& model, const tinygltf::Primitive& primitive, FMesh& mesh)
{
    size_t vertexOffset = mesh.m_vertices.size();

    // Extract vertex attributes
    auto positionIter = primitive.attributes.find("POSITION");
    if (positionIter != primitive.attributes.end())
    {
        ExtractPositions(model, positionIter->second, mesh);
    }

    // Extract normals
    auto normalIter = primitive.attributes.find("NORMAL");
    if (normalIter != primitive.attributes.end())
    {
        ExtractNormals(model, normalIter->second, mesh);
    }

    // Extract UV coordinates
    auto texCoordIter = primitive.attributes.find("TEXCOORD_0");
    if (texCoordIter != primitive.attributes.end())
    {
        ExtractTexCoords(model, texCoordIter->second, mesh);
    }

    // Extract tangents (if present)
    auto tangentIter = primitive.attributes.find("TANGENT");
    if (tangentIter != primitive.attributes.end())
    {
        ExtractTangents(model, tangentIter->second, mesh);
    }

    // Extract the index
    if (primitive.indices >= 0)
    {
        size_t indexStartBefore = mesh.m_indices.size();
        ExtractIndices(model, primitive.indices, mesh);

        // Adjust the index shift
        for (size_t i = indexStartBefore; i < mesh.m_indices.size(); ++i)
        {
            mesh.m_indices[i] += static_cast<unsigned int>(vertexOffset);
        }
    }
}

void GlbModelLoader::CalculateTangentsAndBitangents(std::unique_ptr<FMesh>::element_type& mesh)
{
    for (Vertex_PCUTBN& vertex : mesh.m_vertices)
    {
        if (vertex.m_normal != Vec3::ZERO && vertex.m_tangent != Vec3::ZERO)
        {
            vertex.m_bitangent = CrossProduct3D(vertex.m_normal, vertex.m_tangent).GetNormalized();
        }
    }
}

/**
 * Extracts position data from a given GLTF accessor and populates the `mesh`
 * object with vertex positions.
 *
 * @param model The GLTF model containing the accessor and associated buffer data.
 * @param accessorIndex The index of the accessor within the GLTF model that specifies
 *                      the position data.
 * @param mesh The mesh object to be populated with the extracted vertex position data.
 */
void GlbModelLoader::ExtractPositions(const tinygltf::Model& model, int accessorIndex, FMesh& mesh)
{
    const auto& accessor   = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer     = model.buffers[bufferView.buffer];

    GUARANTEE_OR_DIE(accessor.type == TINYGLTF_TYPE_VEC3, "Position accessor must be VEC3")
    GUARANTEE_OR_DIE(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "Position must be float")

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

/**
 * Extracts normal vectors from the specified accessor in the GLTF model and
 * populates the `mesh` object with the normalized normals for each vertex.
 *
 * @param model The GLTF model containing the normal attribute data.
 * @param accessorIndex The index of the accessor within the model that specifies
 *                      the normal data for the vertices.
 * @param mesh The mesh object to be updated with the extracted and normalized
 *             normal vectors corresponding to each vertex.
 */
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
        mesh.m_vertices[currentSize + i].m_normal = normal.GetNormalized();
    }
}

/**
 * Extracts texture coordinate (UV) data from a GLTF model and populates the
 * corresponding UV coordinates in the provided `mesh` object.
 *
 * @param model The GLTF model containing accessor and buffer data for the UVs.
 * @param accessorIndex The index of the accessor in the GLTF model that points
 *                      to UV data.
 * @param mesh The mesh object to be populated with the extracted UV coordinates.
 */
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

/**
 * Extracts tangent vectors from a specified accessor in the provided GLTF model
 * and populates the corresponding tangent data in the vertices of the given mesh object.
 *
 * @param model The GLTF model containing the tangent data.
 * @param accessorIndex The index of the accessor referencing the tangent data.
 * @param mesh The mesh object to be populated with the extracted tangent vectors,
 *             with the tangents normalized during the process.
 */
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
        mesh.m_vertices[currentSize + i].m_tangent = tangent.GetNormalized();
        // The fourth component (w) is usually used to calculate the direction of the bi-tangent,
        // which is ignored here for the time being
    }
}

/**
 * Extracts the index data from a specific accessor within a GLTF model and appends
 * it to the indices vector of the provided mesh object.
 *
 * @param model The GLTF model containing the accessor data.
 * @param accessorIndex The index of the accessor within the model that contains index data.
 * @param mesh The mesh object where the extracted indices will be stored.
 */
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
        ERROR_AND_DIE("Unsupported index component type")
    }
}

/**
 * Extracts material properties from the provided GLTF model and populates
 * the material list in the specified `mesh` object. Each material is processed
 * and translated into the application's internal material representation.
 *
 * @param model The GLTF model containing the material definitions.
 * @param mesh The mesh object to be populated with the extracted materials.
 *             The `materials` vector of the mesh is filled with processed materials.
 */
void GlbModelLoader::ExtractMaterials(const tinygltf::Model& model, FMesh& mesh)
{
    mesh.materials.reserve(model.materials.size());

    for (size_t i = 0; i < model.materials.size(); ++i)
    {
        const auto& gltfMaterial = model.materials[i];
        FMaterial   material;
        material.name = gltfMaterial.name.empty() ? "Material_" + std::to_string(i) : gltfMaterial.name;

        ProcessMaterial(gltfMaterial, material, model);
        mesh.materials.push_back(std::move(material));
    }
}

/**
 * Processes the material data from a glTF model and populates the provided `material`
 * object with rendering parameters, textures, and other properties.
 *
 * @param gltfMaterial The material object from the glTF model containing
 *                     data such as PBR factors, emissive factors, and textures.
 * @param material The `FMaterial` object to be populated with extracted material
 *                 properties, including factors, texture references, alpha mode,
 *                 and other parameters.
 * @param model The glTF model containing the material and texture data to
 *              resolve texture indices and other related information.
 */
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
    // 1. Albedo texture
    if (pbr.baseColorTexture.index >= 0)
    {
        auto albedoTexture = ExtractTextureFromInfo(pbr.baseColorTexture, model, "albedo");
        if (albedoTexture)
        {
            material.textureCoordSets[EMaterialChannel::Albedo] = pbr.baseColorTexture.texCoord;
            material.SetTexture(EMaterialChannel::Albedo, std::move(albedoTexture));
        }
    }

    // 2. Metallic-Roughness texture
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

    // 3. Normal texture
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

    // 4. AO textures
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

    // 5. Emission textures
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

/**
 * Creates a texture object from a GLTF image by converting its pixel data
 * into an appropriate format and wrapping it in a smart pointer.
 * The image data is converted into grayscale, RGB, or RGBA format
 * depending on the number of components in the GLTF image.
 *
 * @param gltfImage The tinygltf::Image object containing image data, dimensions,
 *                  and component information.
 * @param debugName A string used as a debug name for the texture being created.
 *                  This is typically used for debugging or profiling.
 *
 * @return A unique_ptr to a Texture object if the conversion is successful,
 *         or nullptr if the process fails (e.g., invalid image data or unsupported format).
 */
std::unique_ptr<Texture> GlbModelLoader::CreateTextureFromGLTFImage(const tinygltf::Image& gltfImage, const std::string& debugName)
{
    UNUSED(debugName)
    if (gltfImage.image.empty())
    {
        return nullptr;
    }

    // Create an Image object for the engine
    IntVec2 dimensions(gltfImage.width, gltfImage.height);
    Image   engineImage(dimensions, Rgba8::WHITE); // 创建具有正确尺寸的Image

    // Set the pixel data manually
    const unsigned char* srcData     = gltfImage.image.data();
    int                  totalPixels = gltfImage.width * gltfImage.height;

    // Converts pixel format based on the number of channels
    if (gltfImage.component == 1)
    {
        // Grayscale graph
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

    // Create textures using the existing IRenderer interface and immediately wrap them as smart pointers
    Texture* rawTexture = m_renderer->CreateTextureFromImage(engineImage);
    if (rawTexture)
    {
        // Immediately packaged as a smart pointer to ensure RAII and ownership transfer
        return std::unique_ptr<Texture>{rawTexture};
    }

    return nullptr;
}

/**
 * Extracts a texture from the given texture information in a GLTF model,
 * validating its existence and creating a texture instance from the associated
 * image data. Returns a unique pointer to the created texture, or null if the
 * texture or its source image is invalid.
 *
 * @param textureInfo Referenced texture information containing the index of
 *                    the texture in the GLTF model and other metadata.
 * @param model The GLTF model containing the texture and associated images.
 * @param channelName The name of the texture channel (e.g., "albedo",
 *                    "metallic_roughness") used for debugging.
 * @return A unique pointer to the extracted texture object, or nullptr if the
 *         texture or its source image is invalid.
 */
std::unique_ptr<Texture> GlbModelLoader::ExtractTextureFromInfo(const tinygltf::TextureInfo& textureInfo, const tinygltf::Model& model, const std::string& channelName)
{
    if (textureInfo.index < 0 || textureInfo.index >= (int)model.textures.size())
    {
        return nullptr;
    }

    const auto& texture = model.textures[textureInfo.index];
    if (texture.source < 0 || texture.source >= (int)model.images.size())
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
