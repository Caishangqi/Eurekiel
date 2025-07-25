﻿#pragma once
#include "ModelLoader.hpp"
#include "Engine/Core/StringUtils.hpp"

class ObjModelLoader : public ModelLoader
{
public:
    ObjModelLoader(IRenderer* renderer);
    std::unique_ptr<FMesh> Load(const ResourceLocation& location, const std::string& filePath) override;
    int                    GetPriority() const override { return 100; }
    std::string            GetLoaderName() const override { return "ObjModelLoader"; }
    bool                   CanLoad(const std::string& extension) const override;

public:
    // Helper functions
    void        ValidateMeshData(const FMesh& mesh) const;
    static void CalculateTangentSpace(FMesh& mesh);
    static bool IsDefaultNormal(const Vec3& normal);
    static void GenerateNormalsIfNeeded(FMesh& mesh);
    static bool HasValidUVs(const Vertex_PCUTBN& v0, const Vertex_PCUTBN& v1, const Vertex_PCUTBN& v2);
    static void CalculateTangentSpaceForTriangle(Vertex_PCUTBN& v0, Vertex_PCUTBN& v1, Vertex_PCUTBN& v2);
    static void OrthonormalizeVertexTangentSpace(Vertex_PCUTBN& vertex);

private:
    std::unique_ptr<FMesh> LoadObjModel(const std::string& filePath);

    // Optimized parsing function
    void ParseObjContentOptimized(const std::string& content, FMesh& mesh, std::vector<std::string>& faceLines);
    void ParseVertex(const char*& ptr, const char* end, FMesh& mesh);
    void ParseNormal(const char*& ptr, const char* end, FMesh& mesh);
    void ParseTexCoord(const char*& ptr, const char* end, FMesh& mesh);

    void ProcessFacesOptimized(FMesh& mesh, const std::vector<std::string>& faceLines);
    void ProcessMetaData(std::unique_ptr<FMesh>& mesh);

    [[deprecated]] void ProcessVertex(FMesh& mesh, std::string& data);
    [[deprecated]] void ProcessNormal(FMesh& mesh, std::string& data);
    [[deprecated]] void ProcessTextureCoords(FMesh& mesh, std::string& data);
    [[deprecated]] void ProcessFaces(FMesh& mesh, Strings& data);
    [[deprecated]] void ProcessExtraSpace(std::string& inData);
};
