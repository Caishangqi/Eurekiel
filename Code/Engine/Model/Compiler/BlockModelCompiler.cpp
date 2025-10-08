#include "BlockModelCompiler.hpp"
#include "../../Resource/Model/ModelResource.hpp"
#include "../../Renderer/Model/BlockRenderMesh.hpp"
#include "../../Resource/Atlas/AtlasManager.hpp"
#include "../../Resource/Atlas/TextureAtlas.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Core/Rgba8.hpp"
#include <array>

using namespace enigma::resource;
using namespace enigma::voxel;
using namespace enigma::core;

namespace enigma::renderer::model
{
    std::shared_ptr<RenderMesh> BlockModelCompiler::Compile(std::shared_ptr<resource::model::ModelResource> model, const CompilerContext& context)
    {
        if (!model)
        {
            LogError("BlockModelCompiler", "Cannot compile null model");
            return nullptr;
        }

        SetCompilerContext(context);

        // Get resolved textures and elements (with parent inheritance)
        auto resolvedTextures = model->GetResolvedTextures();
        auto resolvedElements = model->GetResolvedElements();

        if (context.enableLogging)
        {
            LogInfo("BlockModelCompiler", "Compiling block model with %zu textures and %zu elements",
                    resolvedTextures.size(), resolvedElements.size());
        }

        // Create block render mesh
        auto blockMesh = std::make_shared<BlockRenderMesh>();

        // BlockModelCompiler is pure - it only compiles existing ModelElements
        if (resolvedElements.empty())
        {
            LogError("BlockModelCompiler", "No elements to compile! ModelResource should contain geometry.");
            return blockMesh; // Return empty mesh
        }

        // Compile all elements into faces
        for (const auto& element : resolvedElements)
        {
            CompileElementToFaces(element, resolvedTextures, blockMesh);
        }

        if (context.enableLogging)
        {
            LogInfo("BlockModelCompiler", "Generated block mesh with %zu faces", blockMesh->faces.size());
        }

        return blockMesh;
    }

    void BlockModelCompiler::CompileElementToFaces(const resource::model::ModelElement&                     element,
                                                   const std::map<std::string, resource::ResourceLocation>& resolvedTextures,
                                                   std::shared_ptr<BlockRenderMesh>                         blockMesh)
    {
        if (m_context.enableLogging)
        {
            LogInfo("BlockModelCompiler", "Compiling element with %zu faces (from: %.1f,%.1f,%.1f, to: %.1f,%.1f,%.1f)",
                    element.faces.size(),
                    element.from.x, element.from.y, element.from.z,
                    element.to.x, element.to.y, element.to.z);
        }

        // Assignment 1 face colors (from requirements)
        const std::map<std::string, Rgba8> faceColors = {
            {"down", Rgba8::WHITE}, // down - white
            {"up", Rgba8::WHITE}, // up - white  
            {"north", Rgba8(200, 200, 200)}, // north - darker grey
            {"south", Rgba8(200, 200, 200)}, // south - darker grey
            {"west", Rgba8(230, 230, 230)}, // west - light grey
            {"east", Rgba8(230, 230, 230)} // east - light grey
        };

        // Compile each face of the element
        for (const auto& [faceDirection, modelFace] : element.faces)
        {
            // Resolve texture reference (e.g., "#down" -> actual texture location)
            ResourceLocation textureLocation;
            std::string      textureRef = modelFace.texture;

            if (textureRef.find("#") == 0) // C++17 compatible check for starts_with("#")
            {
                // Texture variable reference
                std::string varName = textureRef.substr(1);
                auto        it      = resolvedTextures.find(varName);
                if (it != resolvedTextures.end())
                {
                    textureLocation = it->second;
                }
                else
                {
                    LogWarn("BlockModelCompiler", "Unresolved texture variable: %s", textureRef.c_str());
                    textureLocation = ResourceLocation("engine", "missingno");
                }
            }
            else
            {
                // Direct texture reference
                textureLocation = ResourceLocation::Parse(textureRef);
            }

            // Get UV coordinates from atlas
            auto [uvMin, uvMax] = GetAtlasUV(textureLocation);

            // Convert face direction string to Direction enum
            Direction direction = StringToDirection(faceDirection);

            // Create render face using element dimensions
            RenderFace face = CreateElementFace(direction, element, modelFace, uvMin, uvMax);

            // Apply Assignment 1 face colors
            auto  colorIt   = faceColors.find(faceDirection);
            Rgba8 faceColor = (colorIt != faceColors.end()) ? colorIt->second : Rgba8::WHITE;

            for (auto& vertex : face.vertices)
            {
                vertex.m_color = faceColor;
            }

            blockMesh->AddFace(face);
        }
    }

    RenderFace BlockModelCompiler::CreateElementFace(voxel::Direction                                   direction,
                                                     const resource::model::ModelElement&               element,
                                                     [[maybe_unused]] const resource::model::ModelFace& modelFace,
                                                     const Vec2&                                        uvMin, const Vec2& uvMax) const
    {
        RenderFace face(direction);

        // Convert from Minecraft coordinates (0-16) to normalized coordinates (0-1)
        Vec3 from = element.from / 16.0f;
        Vec3 to   = element.to / 16.0f;

        // Create quad vertices based on direction and element bounds
        // World coordinate system: +X=East, +Y=North, +Z=Up
        // Note: Vertices ordered counter-clockwise for front face
        switch (direction)
        {
        case Direction::DOWN: // Bottom face (-Z, world bottom) - looking up at it
            face.vertices = {
                Vertex_PCU(Vec3(from.x, from.y, from.z), Rgba8::WHITE, Vec2(uvMin.x, uvMax.y)),
                Vertex_PCU(Vec3(from.x, to.y, from.z), Rgba8::WHITE, Vec2(uvMin.x, uvMin.y)),
                Vertex_PCU(Vec3(to.x, to.y, from.z), Rgba8::WHITE, Vec2(uvMax.x, uvMin.y)),
                Vertex_PCU(Vec3(to.x, from.y, from.z), Rgba8::WHITE, Vec2(uvMax.x, uvMax.y))
            };
            break;

        case Direction::UP: // Top face (+Z, world top) - looking down at it
            face.vertices = {
                Vertex_PCU(Vec3(from.x, from.y, to.z), Rgba8::WHITE, Vec2(uvMin.x, uvMin.y)),
                Vertex_PCU(Vec3(to.x, from.y, to.z), Rgba8::WHITE, Vec2(uvMax.x, uvMin.y)),
                Vertex_PCU(Vec3(to.x, to.y, to.z), Rgba8::WHITE, Vec2(uvMax.x, uvMax.y)),
                Vertex_PCU(Vec3(from.x, to.y, to.z), Rgba8::WHITE, Vec2(uvMin.x, uvMax.y))
            };
            break;

        case Direction::NORTH: // North face (+Y, north)
            face.vertices = {
                Vertex_PCU(Vec3(from.x, to.y, from.z), Rgba8::WHITE, Vec2(uvMax.x, uvMin.y)),
                Vertex_PCU(Vec3(from.x, to.y, to.z), Rgba8::WHITE, Vec2(uvMax.x, uvMax.y)),
                Vertex_PCU(Vec3(to.x, to.y, to.z), Rgba8::WHITE, Vec2(uvMin.x, uvMax.y)),
                Vertex_PCU(Vec3(to.x, to.y, from.z), Rgba8::WHITE, Vec2(uvMin.x, uvMin.y))
            };
            break;

        case Direction::SOUTH: // South face (-Y, south)  
            face.vertices = {
                Vertex_PCU(Vec3(to.x, from.y, from.z), Rgba8::WHITE, Vec2(uvMax.x, uvMin.y)),
                Vertex_PCU(Vec3(to.x, from.y, to.z), Rgba8::WHITE, Vec2(uvMax.x, uvMax.y)),
                Vertex_PCU(Vec3(from.x, from.y, to.z), Rgba8::WHITE, Vec2(uvMin.x, uvMax.y)),
                Vertex_PCU(Vec3(from.x, from.y, from.z), Rgba8::WHITE, Vec2(uvMin.x, uvMin.y))
            };
            break;

        case Direction::WEST: // West face (-X, west)
            face.vertices = {
                Vertex_PCU(Vec3(from.x, from.y, from.z), Rgba8::WHITE, Vec2(uvMax.x, uvMin.y)),
                Vertex_PCU(Vec3(from.x, from.y, to.z), Rgba8::WHITE, Vec2(uvMax.x, uvMax.y)),
                Vertex_PCU(Vec3(from.x, to.y, to.z), Rgba8::WHITE, Vec2(uvMin.x, uvMax.y)),
                Vertex_PCU(Vec3(from.x, to.y, from.z), Rgba8::WHITE, Vec2(uvMin.x, uvMin.y))
            };
            break;

        case Direction::EAST: // East face (+X, east)
            face.vertices = {
                Vertex_PCU(Vec3(to.x, to.y, from.z), Rgba8::WHITE, Vec2(uvMax.x, uvMin.y)),
                Vertex_PCU(Vec3(to.x, to.y, to.z), Rgba8::WHITE, Vec2(uvMax.x, uvMax.y)),
                Vertex_PCU(Vec3(to.x, from.y, to.z), Rgba8::WHITE, Vec2(uvMin.x, uvMax.y)),
                Vertex_PCU(Vec3(to.x, from.y, from.z), Rgba8::WHITE, Vec2(uvMin.x, uvMin.y))
            };
            break;
        }

        // Create indices for two triangles (quad)
        face.indices = {0, 1, 2, 0, 2, 3};

        // Set face properties
        face.cullDirection = direction;
        face.isOpaque      = true;

        return face;
    }

    std::pair<Vec2, Vec2> BlockModelCompiler::GetAtlasUV(const resource::ResourceLocation& textureLocation) const
    {
        if (!m_atlas)
        {
            LogWarn("BlockModelCompiler", "No atlas available for texture: %s", textureLocation.ToString().c_str());
            return {Vec2(0, 0), Vec2(1, 1)};
        }

        // Convert model texture reference to actual texture path
        // "simpleminer:block/stone" -> "simpleminer:textures/block/stone"
        ResourceLocation actualTextureLocation = textureLocation;
        std::string      path                  = textureLocation.GetPath();
        if (path.find("textures/") != 0) // Check if path doesn't start with "textures/"
        {
            actualTextureLocation = ResourceLocation(textureLocation.GetNamespace(), "textures/" + path);
        }

        // Debug: Print atlas info
        if (m_context.enableLogging)
        {
            LogInfo("BlockModelCompiler", "Looking for texture: %s -> %s in atlas with %zu sprites",
                    textureLocation.ToString().c_str(), actualTextureLocation.ToString().c_str(), m_atlas->GetAllSprites().size());
        }

        // Find sprite in atlas using the resolved path
        const auto* sprite = m_atlas->FindSprite(actualTextureLocation);
        if (!sprite)
        {
            if (m_context.enableLogging)
            {
                LogWarn("BlockModelCompiler", "Texture not found in atlas: %s (resolved to %s)",
                        textureLocation.ToString().c_str(), actualTextureLocation.ToString().c_str());

                // List all sprites in atlas for debugging
                const auto& allSprites = m_atlas->GetAllSprites();
                for (const auto& spriteInfo : allSprites)
                {
                    LogInfo("BlockModelCompiler", "  Atlas contains sprite: %s", spriteInfo.location.ToString().c_str());
                }
            }
            return {Vec2(0, 0), Vec2(1, 1)};
        }

        if (m_context.enableLogging)
        {
            LogInfo("BlockModelCompiler", "Found texture: %s -> %s",
                    textureLocation.ToString().c_str(), actualTextureLocation.ToString().c_str());
        }

        return {sprite->uvMin, sprite->uvMax};
    }
}
