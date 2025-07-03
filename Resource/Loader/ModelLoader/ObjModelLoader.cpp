#include "ObjModelLoader.hpp"

#include <array>
#include <regex>
#include <sstream>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Math/MathUtils.hpp"

ObjModelLoader::ObjModelLoader(Renderer* renderer) : ModelLoader(renderer)
{
}

std::unique_ptr<FMesh> ObjModelLoader::Load(const ResourceLocation& location, const std::string& filePath)
{
    UNUSED(location)
    return LoadObjModel(filePath);
}

bool ObjModelLoader::CanLoad(const std::string& extension) const
{
    UNUSED(extension)
    // TODO: Check whether or not have meta file and extension matches
    return true;
}

std::unique_ptr<FMesh> ObjModelLoader::LoadObjModel(const std::string& filePath)
{
    // Prepare FMesh
    FMesh mesh;

    // Read the entire file into a string at once
    std::string fileContent;
    FileReadToString(fileContent, filePath);

    if (fileContent.empty())
    {
        ERROR_AND_DIE("Unable to read OBJ file:" + filePath)
    }

    // Preallocate container (better estimate based on file size)
    size_t fileSize       = fileContent.size();
    size_t estimatedLines = fileSize / 20; // Average 20 characters per line

    mesh.vertexPosition.reserve(estimatedLines / 8); // Estimate the number of v rows
    mesh.vertexNormal.reserve(estimatedLines / 10); // Estimate the number of vn rows
    mesh.uvTexCoords.reserve(estimatedLines / 10); // Estimate the number of vt rows

    std::vector<std::string> faceLines;
    faceLines.reserve(estimatedLines / 4); // Estimate the number of face rows

    // Use efficient string parsing, avoiding regular expressions
    ParseObjContentOptimized(fileContent, mesh, faceLines);

    // Process face data
    ProcessFacesOptimized(mesh, faceLines);

    // Generate normal and tangent space
    GenerateNormalsIfNeeded(mesh);

    return std::make_unique<FMesh>(mesh);
}

// Optimized content parsing function - avoid line-by-line splitting and regular expressions
void ObjModelLoader::ParseObjContentOptimized(const std::string& content, FMesh& mesh, std::vector<std::string>& faceLines)
{
    const char* ptr = content.c_str();
    const char* end = ptr + content.size();

    while (ptr < end)
    {
        // Skip whitespace characters
        while (ptr < end && (*ptr == ' ' || *ptr == '\t')) ++ptr;

        // Skip empty lines
        if (ptr >= end || *ptr == '\n' || *ptr == '\r')
        {
            // Jump to the next line
            while (ptr < end && (*ptr == '\n' || *ptr == '\r')) ++ptr;
            continue;
        }

        // Skip comment lines
        if (*ptr == '#')
        {
            // Jump to the end of the line
            while (ptr < end && *ptr != '\n' && *ptr != '\r') ++ptr;
            continue;
        }

        // Parse command type
        if (ptr + 1 < end)
        {
            if (*ptr == 'v')
            {
                if (*(ptr + 1) == ' ')
                {
                    //Vertex position "v x y z"
                    ptr += 2;
                    ParseVertex(ptr, end, mesh);
                }
                else if (*(ptr + 1) == 'n' && *(ptr + 2) == ' ')
                {
                    // Normal "vn x y z"
                    ptr += 3;
                    ParseNormal(ptr, end, mesh);
                }
                else if (*(ptr + 1) == 't' && *(ptr + 2) == ' ')
                {
                    // Texture coordinates "vt u v"
                    ptr += 3;
                    ParseTexCoord(ptr, end, mesh);
                }
                else
                {
                    // Skip unknown v commands
                    while (ptr < end && *ptr != '\n' && *ptr != '\r') ++ptr;
                }
            }
            else if (*ptr == 'f' && *(ptr + 1) == ' ')
            {
                // Face "f ..."
                const char* lineStart = ptr;
                while (ptr < end && *ptr != '\n' && *ptr != '\r') ++ptr;
                faceLines.emplace_back(lineStart, ptr - lineStart);
            }
            else
            {
                // Skip other commands
                while (ptr < end && *ptr != '\n' && *ptr != '\r') ++ptr;
            }
        }
        else
        {
            // Skip single character lines
            while (ptr < end && *ptr != '\n' && *ptr != '\r') ++ptr;
        }

        // Jump to the next line
        while (ptr < end && (*ptr == '\n' || *ptr == '\r')) ++ptr;
    }
}

// Efficient floating point parsing
inline float FastAtof(const char*& ptr, const char* end)
{
    // Skip leading spaces
    while (ptr < end && (*ptr == ' ' || *ptr == '\t')) ++ptr;

    if (ptr >= end) return 0.0f;

    bool negative = false;
    if (*ptr == '-')
    {
        negative = true;
        ++ptr;
    }
    else if (*ptr == '+')
    {
        ++ptr;
    }

    float result = 0.0f;

    // integer part
    while (ptr < end && *ptr >= '0' && *ptr <= '9')
    {
        result = result * 10.0f + (float)(*ptr - '0');
        ++ptr;
    }

    // Decimal part
    if (ptr < end && *ptr == '.')
    {
        ++ptr;
        float fraction = 0.1f;
        while (ptr < end && *ptr >= '0' && *ptr <= '9')
        {
            result += (float)(*ptr - '0') * fraction;
            fraction *= 0.1f;
            ++ptr;
        }
    }
    return negative ? -result : result;
}

inline int FastParseInt(const char*& ptr, const char* end)
{
    int result = 0;
    while (ptr < end && *ptr >= '0' && *ptr <= '9')
    {
        result = result * 10 + (*ptr - '0');
        ++ptr;
    }
    return result;
}

void ObjModelLoader::ParseVertex(const char*& ptr, const char* end, FMesh& mesh)
{
    Vec3 position;
    position.x = FastAtof(ptr, end);
    position.y = FastAtof(ptr, end);
    position.z = FastAtof(ptr, end);
    mesh.vertexPosition.push_back(position);
}

// Optimized normal parsing
void ObjModelLoader::ParseNormal(const char*& ptr, const char* end, FMesh& mesh)
{
    Vec3 normal;
    normal.x = FastAtof(ptr, end);
    normal.y = FastAtof(ptr, end);
    normal.z = FastAtof(ptr, end);
    mesh.vertexNormal.push_back(normal);
}

// Optimized texture coordinate parsing
void ObjModelLoader::ParseTexCoord(const char*& ptr, const char* end, FMesh& mesh)
{
    Vec2 uv;
    uv.x = FastAtof(ptr, end);
    uv.y = FastAtof(ptr, end);
    mesh.uvTexCoords.push_back(uv);
}

void ObjModelLoader::ProcessFacesOptimized(FMesh& mesh, const std::vector<std::string>& faceLines)
{
    // Use local definitions to avoid header file dependency issues
    struct LocalFaceVertex
    {
        int posIndex    = -1;
        int uvIndex     = -1;
        int normalIndex = -1;
    };

    // Preallocate triangle container
    std::vector<std::array<LocalFaceVertex, 3>> allTriangles;
    allTriangles.reserve(faceLines.size() * 2); // Assume Most of them are probably quad

    for (const std::string& faceLine : faceLines)
    {
        std::vector<LocalFaceVertex> faceVertices;
        faceVertices.reserve(8); // Supports up to 8 polygons

        //Quickly parse the face line
        const char* ptr = faceLine.c_str() + 1; // Skip 'f'
        const char* end = ptr + faceLine.size() - 1;

        while (ptr < end)
        {
            // Skip spaces
            while (ptr < end && *ptr == ' ') ++ptr;
            if (ptr >= end) break;

            LocalFaceVertex vertex;

            // Parse vertex index
            vertex.posIndex = FastParseInt(ptr, end) - 1;

            if (ptr < end && *ptr == '/')
            {
                ++ptr;
                if (ptr < end && *ptr != '/')
                {
                    vertex.uvIndex = FastParseInt(ptr, end) - 1;
                }
                if (ptr < end && *ptr == '/')
                {
                    ++ptr;
                    if (ptr < end && *ptr != ' ')
                    {
                        vertex.normalIndex = FastParseInt(ptr, end) - 1;
                    }
                }
            }

            faceVertices.push_back(vertex);
        }

        // Triangulate
        if (faceVertices.size() >= 3)
        {
            for (size_t i = 1; i < faceVertices.size() - 1; ++i)
            {
                std::array<LocalFaceVertex, 3> triangle = {
                    faceVertices[0],
                    faceVertices[i],
                    faceVertices[i + 1]
                };
                allTriangles.push_back(triangle);
            }
        }
    }

    // Generate vertex data - process it here directly to avoid type conversion issues
    mesh.vertices.clear();
    mesh.vertices.reserve(allTriangles.size() * 3);

    // Default Value
    const Vec3  defaultPosition(0.0f, 0.0f, 0.0f);
    const Vec3  defaultNormal(0.0f, 0.0f, 1.0f);
    const Vec3  defaultTangent(1.0f, 0.0f, 0.0f);
    const Vec3  defaultBinormal(0.0f, 1.0f, 0.0f);
    const Vec2  defaultUV(0.0f, 0.0f);
    const Rgba8 defaultColor(255, 255, 255, 255);

    for (const auto& triangle : allTriangles)
    {
        for (int i = 0; i < 3; ++i)
        {
            const LocalFaceVertex& faceVertex = triangle[i];

            Vertex_PCUTBN vertex;

            // Get data safely
            vertex.m_position = (faceVertex.posIndex >= 0 && faceVertex.posIndex < (int)mesh.vertexPosition.size())
                                    ? mesh.vertexPosition[faceVertex.posIndex]
                                    : defaultPosition;

            vertex.m_normal = (faceVertex.normalIndex >= 0 && faceVertex.normalIndex < (int)mesh.vertexNormal.size())
                                  ? mesh.vertexNormal[faceVertex.normalIndex]
                                  : defaultNormal;

            vertex.m_uvTexCoords = (faceVertex.uvIndex >= 0 && faceVertex.uvIndex < (int)mesh.uvTexCoords.size())
                                       ? mesh.uvTexCoords[faceVertex.uvIndex]
                                       : defaultUV;

            vertex.m_color     = defaultColor;
            vertex.m_tangent   = defaultTangent;
            vertex.m_bitangent = defaultBinormal;

            mesh.vertices.push_back(vertex);
        }
    }
}

void ObjModelLoader::ProcessVertex(FMesh& mesh, std::string& data)
{
    Vec3    position;
    Strings values = SplitStringOnDelimiter(data, ' ');
    position.x     = (float)atof(values[1].c_str());
    position.y     = (float)atof(values[2].c_str());
    position.z     = (float)atof(values[3].c_str());
    mesh.vertexPosition.push_back(position);
}

void ObjModelLoader::ProcessNormal(FMesh& mesh, std::string& data)
{
    Vec3    normal;
    Strings values = SplitStringOnDelimiter(data, ' ');
    normal.x       = (float)atof(values[1].c_str());
    normal.y       = (float)atof(values[2].c_str());
    normal.z       = (float)atof(values[3].c_str());
    mesh.vertexNormal.push_back(normal);
}

void ObjModelLoader::ProcessTextureCoords(FMesh& mesh, std::string& data)
{
    Vec2    uvCoords;
    Strings values = SplitStringOnDelimiter(data, ' ');
    uvCoords.x     = (float)atof(values[1].c_str());
    uvCoords.y     = (float)atof(values[2].c_str());
    mesh.uvTexCoords.push_back(uvCoords);
}

void ObjModelLoader::ProcessFaces(FMesh& mesh, Strings& data)
{
    struct FaceVertex
    {
        int posIndex    = -1; //Vertex position index
        int uvIndex     = -1; // Texture coordinate index
        int normalIndex = -1; // Normal index
    };

    // Parse a single vertex index string (like "1//2" or "1/2/3" or "1")
    auto parseFaceVertex = [](const std::string& vertexStr) -> FaceVertex
    {
        FaceVertex vertex;

        // 分割索引字符串
        std::vector<std::string> indices;
        std::stringstream        ss(vertexStr);
        std::string              token;

        while (std::getline(ss, token, '/'))
        {
            indices.push_back(token);
        }

        // Parse indexes in different formats
        if (!indices.empty() && !indices[0].empty())
        {
            vertex.posIndex = std::atoi(indices[0].c_str()) - 1; // OBJ index starts from 1 and is converted to start from 0
        }

        if (indices.size() >= 2 && !indices[1].empty())
        {
            vertex.uvIndex = std::atoi(indices[1].c_str()) - 1;
        }

        if (indices.size() >= 3 && !indices[2].empty())
        {
            vertex.normalIndex = std::atoi(indices[2].c_str()) - 1;
        }

        return vertex;
    };

    // Triangulate polygonal face (fan triangulation: f 1 2 3 4 5 -> 123, 134, 145)
    auto triangulatePolygon = [](const std::vector<FaceVertex>& polygon) -> std::vector<std::array<FaceVertex, 3>>
    {
        std::vector<std::array<FaceVertex, 3>> triangles;

        if (polygon.size() < 3) return triangles;

        // Fan-shaped triangulation: With the first vertex as the center, connect all adjacent vertex pairs
        for (size_t i = 1; i < polygon.size() - 1; ++i)
        {
            std::array<FaceVertex, 3> triangle = {
                polygon[0], // First vertex
                polygon[i], // Current vertex
                polygon[i + 1] // Next vertex
            };
            triangles.push_back(triangle);
        }

        return triangles;
    };

    // Process all faces
    std::vector<std::array<FaceVertex, 3>> allTriangles;

    for (const std::string& faceLine : data)
    {
        // Split face row: "f v1//n1 v2//n2 v3//n3 ..."
        Strings faceTokens = SplitStringOnDelimiter(faceLine, ' ');

        if (faceTokens.size() < 4) continue; // At least "f" + 3 vertices are required

        // parse all vertices of the face (skipping the first "f" marker)
        std::vector<FaceVertex> faceVertices;
        for (size_t i = 1; i < faceTokens.size(); ++i)
        {
            FaceVertex vertex = parseFaceVertex(faceTokens[i]);
            faceVertices.push_back(vertex);
        }

        // Triangulate this face
        auto triangles = triangulatePolygon(faceVertices);
        allTriangles.insert(allTriangles.end(), triangles.begin(), triangles.end());
    }

    // Generate the final Vertex_PCUTBN vertex data
    mesh.vertices.clear();
    mesh.vertices.reserve(allTriangles.size() * 3);

    // Default Value
    const Vec3  defaultPosition(0.0f, 0.0f, 0.0f);
    const Vec3  defaultNormal(0.0f, 0.0f, 1.0f); // 向上法线
    const Vec3  defaultTangent(1.0f, 0.0f, 0.0f); // 向右切线
    const Vec3  defaultBinormal(0.0f, 1.0f, 0.0f); // 向前副法线
    const Vec2  defaultUV(0.0f, 0.0f);
    const Rgba8 defaultColor(255, 255, 255, 255); // 白色

    for (const auto& triangle : allTriangles)
    {
        for (int i = 0; i < 3; ++i)
        {
            const FaceVertex& faceVertex = triangle[i];

            // Get location data
            Vec3 position = defaultPosition;
            if (faceVertex.posIndex >= 0 && faceVertex.posIndex < (int)mesh.vertexPosition.size())
            {
                position = mesh.vertexPosition[faceVertex.posIndex];
            }

            // Get normal data
            Vec3 normal = defaultNormal;
            if (faceVertex.normalIndex >= 0 && faceVertex.normalIndex < (int)mesh.vertexNormal.size())
            {
                normal = mesh.vertexNormal[faceVertex.normalIndex];
            }

            // Get UV data (texture coordinates)
            Vec2 uv = defaultUV;
            if (faceVertex.uvIndex >= 0 && faceVertex.uvIndex < (int)mesh.uvTexCoords.size())
            {
                uv = mesh.uvTexCoords[faceVertex.uvIndex];
            }

            Vertex_PCUTBN vertex;
            vertex.m_position    = position;
            vertex.m_color       = defaultColor;
            vertex.m_uvTexCoords = uv;
            vertex.m_tangent     = defaultTangent;
            vertex.m_bitangent   = defaultBinormal;
            vertex.m_normal      = normal;

            mesh.vertices.push_back(vertex);
        }
    }

    // Verify the generated data
    ValidateMeshData(mesh);
}

void ObjModelLoader::ValidateMeshData(const FMesh& mesh) const
{
    // Check if the number of vertices is a multiple of 3 (triangle)
    if (mesh.vertices.size() % 3 != 0)
    {
        ERROR_AND_DIE("The number of vertices in the OBJ model is not a multiple of 3 and cannot form a complete triangle")
    }

    // Output loading statistics
    // int triangleCount = (int)mesh.vertices.size() / 3;
    // You can add log output here, for example:
    // DebuggerPrintf("Successfully loaded OBJ model: %d triangles, %d vertices\n", triangleCount, (int)mesh.vertices.size());
}

void ObjModelLoader::GenerateNormalsIfNeeded(FMesh& mesh) const
{
    // If the original data has no normals, you can calculate the normals for each triangle
    if (mesh.vertexNormal.empty() && mesh.vertices.size() >= 3)
    {
        for (size_t i = 0; i < mesh.vertices.size(); i += 3)
        {
            // Calculate triangle normal
            Vec3 v0 = mesh.vertices[i].m_position;
            Vec3 v1 = mesh.vertices[i + 1].m_position;
            Vec3 v2 = mesh.vertices[i + 2].m_position;

            Vec3 edge1  = v1 - v0;
            Vec3 edge2  = v2 - v0;
            Vec3 normal = CrossProduct3D(edge1, edge2);
            normal      = normal.GetNormalized();

            // Set normals for the three vertices of this triangle
            mesh.vertices[i].m_normal     = normal;
            mesh.vertices[i + 1].m_normal = normal;
            mesh.vertices[i + 2].m_normal = normal;
        }
    }

    // Calculate tangent space
    CalculateTangentSpace(mesh);
}

void ObjModelLoader::CalculateTangentSpace(FMesh& mesh) const
{
    // Only calculate if the number of vertices is a multiple of 3
    if (mesh.vertices.size() % 3 != 0) return;

    for (size_t i = 0; i < mesh.vertices.size(); i += 3)
    {
        Vertex_PCUTBN& v0 = mesh.vertices[i];
        Vertex_PCUTBN& v1 = mesh.vertices[i + 1];
        Vertex_PCUTBN& v2 = mesh.vertices[i + 2];

        // Calculate edge vector
        Vec3 edge1 = v1.m_position - v0.m_position;
        Vec3 edge2 = v2.m_position - v0.m_position;

        // Calculate UV difference
        Vec2 deltaUV1 = v1.m_uvTexCoords - v0.m_uvTexCoords;
        Vec2 deltaUV2 = v2.m_uvTexCoords - v0.m_uvTexCoords;

        // Avoid division by zero
        float denominator = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (abs(denominator) < 0.0001f)
        {
            // Use default tangent space
            Vec3 defaultTangent = Vec3(1.0f, 0.0f, 0.0f);
            v0.m_tangent        = defaultTangent;
            v1.m_tangent        = defaultTangent;
            v2.m_tangent        = defaultTangent;

            Vec3 defaultBitangent = Vec3(0.0f, 1.0f, 0.0f);
            v0.m_bitangent        = defaultBitangent;
            v1.m_bitangent        = defaultBitangent;
            v2.m_bitangent        = defaultBitangent;
            continue;
        }

        float f = 1.0f / denominator;

        // Calculate tangent
        Vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
        tangent      = tangent.GetNormalized();

        // Calculate binormal
        Vec3 binormal = f * (-deltaUV2.x * edge1 + deltaUV1.x * edge2);
        binormal      = binormal.GetNormalized();

        // Apply to three vertices
        v0.m_tangent = tangent;
        v1.m_tangent = tangent;
        v2.m_tangent = tangent;

        v0.m_bitangent = binormal;
        v1.m_bitangent = binormal;
        v2.m_bitangent = binormal;
    }
}

void ObjModelLoader::ProcessExtraSpace(std::string& inData)
{
    inData = std::regex_replace(inData, std::regex(R"(^\s+|\s+$)"), "");
    inData = std::regex_replace(inData, std::regex(R"(\s{2,})"), " ");
}
