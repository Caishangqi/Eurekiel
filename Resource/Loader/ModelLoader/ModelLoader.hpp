#pragma once
#include "Engine/Core/Vertex_PCU.hpp"
#include "Engine/Math/Vec4.hpp"
#include "Engine/Resource/Loader/Loader.hpp"

class VertexBuffer;
class IndexBuffer;
struct Vec2;
struct Vec3;
class IRenderer;
class Texture;

enum class EMaterialChannel : uint8_t
{
    Albedo = 0,
    Normal,
    MetallicRoughness,
    Occlusion,
    Emission,
    Specular,
    Gloss,
    Height,
    Opacity,
    COUNT
};

struct FMaterial
{
    std::string name = "DefaultMaterial";

    // PBR基础参数
    Vec4  baseColorFactor   = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
    float metallicFactor    = 1.0f;
    float roughnessFactor   = 1.0f;
    Vec3  emissiveFactor    = Vec3(0.0f, 0.0f, 0.0f);
    float normalScale       = 1.0f;
    float occlusionStrength = 1.0f;
    float alphaCutoff       = 0.5f;

    // 透明度模式
    enum class AlphaMode
    {
        OPAQUE,
        MASK,
        BLEND
    } alphaMode = AlphaMode::OPAQUE;

    bool doubleSided = false;

    // 纹理存储 - 直接存储Texture对象，不需要路径
    std::unordered_map<EMaterialChannel, std::unique_ptr<Texture>> textures;

    // 纹理坐标集映射
    std::unordered_map<EMaterialChannel, int> textureCoordSets;

    // 辅助方法
    bool HasTexture(EMaterialChannel channel) const
    {
        auto it = textures.find(channel);
        return it != textures.end() && it->second != nullptr;
    }

    Texture* GetTexture(EMaterialChannel channel) const
    {
        auto it = textures.find(channel);
        return (it != textures.end()) ? it->second.get() : nullptr;
    }

    void SetTexture(EMaterialChannel channel, std::unique_ptr<Texture> texture)
    {
        textures[channel] = std::move(texture);
    }

    int GetTextureCoordSet(EMaterialChannel channel) const
    {
        auto it = textureCoordSets.find(channel);
        return (it != textureCoordSets.end()) ? it->second : 0;
    }
};

// TODO: Hack the simple Mesh, need refactor in the future
struct FMesh
{
    FMesh();
    ~FMesh();

    // Disable copying, only allow moving
    FMesh(const FMesh&)            = delete;
    FMesh& operator=(const FMesh&) = delete;
    FMesh(FMesh&&)                 = default;
    FMesh& operator=(FMesh&&)      = default;

    // Querying Interfaces
    size_t GetVertexCount() const { return m_vertices.size(); }
    size_t GetIndexCount() const { return m_indices.size(); }
    size_t GetMaterialCount() const { return materials.size(); }
    size_t GetSubMeshCount() const { return subMeshes.size(); }

    const FMaterial* GetMaterial(size_t index) const
    {
        return (index < materials.size()) ? &materials[index] : nullptr;
    }

    void EnsureGPUBuffers(IRenderer* renderer) const;

    bool IsValid() const { return !m_vertices.empty(); }


    // Temporary data storage
    std::vector<Vec3> vertexPosition;
    std::vector<Vec3> vertexNormal;
    std::vector<Vec2> uvTexCoords;

    // Internal Data storage
    std::vector<Vertex_PCUTBN> m_vertices;
    std::vector<unsigned int>  m_indices;

    // Material Information
    std::vector<FMaterial> materials;

    // Submesh Information
    struct SubMesh
    {
        unsigned int materialIndex = 0;
        unsigned int indexStart    = 0;
        unsigned int indexCount    = 0;
        std::string  name;
    };

    std::vector<SubMesh> subMeshes;

    mutable std::shared_ptr<VertexBuffer> vertexBuffer = nullptr;
    mutable std::shared_ptr<IndexBuffer>  indexBuffer  = nullptr;
};

class ModelLoader : public ResourceLoader<FMesh>
{
protected:
    IRenderer* m_renderer; // Cached renderer backend

public:
    ModelLoader(IRenderer* renderer);

    virtual ~ModelLoader() = default;
};
