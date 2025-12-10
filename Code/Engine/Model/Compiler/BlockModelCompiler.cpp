#include "BlockModelCompiler.hpp"
#include "../../Resource/Model/ModelResource.hpp"
#include "../../Renderer/Model/BlockRenderMesh.hpp"
#include "../../Resource/Atlas/AtlasManager.hpp"
#include "../../Resource/Atlas/TextureAtlas.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Core/ErrorWarningAssert.hpp"
#include "../../Math/Vec3.hpp"
#include "../../Core/Rgba8.hpp"
#include <array>

using namespace enigma::resource;
using namespace enigma::voxel;
using namespace enigma::core;
DEFINE_LOG_CATEGORY(LogBlockModelCompiler)

namespace enigma::renderer::model
{
    std::shared_ptr<RenderMesh> BlockModelCompiler::Compile(std::shared_ptr<resource::model::ModelResource> model, const CompilerContext& context)
    {
        if (!model)
        {
            LogError(LogBlockModelCompiler, "[Compile] Cannot compile null model");
            ERROR_RECOVERABLE("BlockModelCompiler::Compile called with null model");
            return nullptr;
        }

        SetCompilerContext(context);

        // Get model location for logging
        std::string modelName    = model->GetMetadata().filePath.generic_string();
        bool        isDebugModel = (modelName.find("stairs") != std::string::npos || modelName.find("slab") != std::string::npos);

        LogInfo(LogBlockModelCompiler, "[Compile] ========== Starting compile for: %s ==========", modelName.c_str());

        // Get resolved elements (with parent inheritance)
        auto resolvedElements = model->GetResolvedElements();
        auto resolvedTextures = model->GetResolvedTextures();

        if (isDebugModel)
        {
            LogInfo(LogBlockModelCompiler, "[DEBUG] Model: %s", modelName.c_str());
            LogInfo(LogBlockModelCompiler, "[DEBUG] Resolved elements: %zu, textures: %zu",
                    resolvedElements.size(), resolvedTextures.size());
        }

        LogInfo(LogBlockModelCompiler, "[Compile] Resolved textures: %zu, elements: %zu",
                resolvedTextures.size(), resolvedElements.size());

        // Log all resolved textures
        for (const auto& [key, texture] : resolvedTextures)
        {
            if (resource::model::IsTextureLocation(texture))
            {
                LogInfo(LogBlockModelCompiler, "  Texture '%s' -> %s (ResourceLocation)",
                        key.c_str(), resource::model::GetTextureLocation(texture).ToString().c_str());
            }
            else
            {
                LogInfo(LogBlockModelCompiler, "  Texture '%s' -> '%s' (Variable reference)",
                        key.c_str(), resource::model::GetVariableReference(texture).c_str());
            }
        }

        // Create block render mesh
        auto blockMesh = std::make_shared<BlockRenderMesh>();

        // BlockModelCompiler is pure - it only compiles existing ModelElements
        if (resolvedElements.empty())
        {
            LogError(LogBlockModelCompiler, "[Compile] CRITICAL: No elements to compile! Model '%s' will have 0 vertices!", modelName.c_str());
            ERROR_RECOVERABLE(Stringf("BlockModelCompiler: Model '%s' has no elements!", modelName.c_str()));
            return blockMesh; // Return empty mesh
        }

        // Log element details
        for (size_t i = 0; i < resolvedElements.size(); ++i)
        {
            const auto& elem = resolvedElements[i];
            LogInfo(LogBlockModelCompiler, "  Element[%zu]: from(%.1f,%.1f,%.1f) to(%.1f,%.1f,%.1f) faces=%zu",
                    i, elem.from.x, elem.from.y, elem.from.z,
                    elem.to.x, elem.to.y, elem.to.z, elem.faces.size());
        }

        // Compile all elements into faces
        // [FIX] Pass ModelResource for Minecraft-style chain texture resolution
        int elementIndex = 0;
        for (const auto& element : resolvedElements)
        {
            if (isDebugModel)
            {
                LogInfo(LogBlockModelCompiler, "[DEBUG STAIRS] Compiling element[%d]: from(%.1f,%.1f,%.1f) to(%.1f,%.1f,%.1f) faces=%zu",
                        elementIndex, element.from.x, element.from.y, element.from.z,
                        element.to.x, element.to.y, element.to.z, element.faces.size());
            }

            CompileElementToFaces(element, model, blockMesh);

            if (isDebugModel)
            {
                LogInfo(LogBlockModelCompiler, "[DEBUG STAIRS] After element[%d]: total faces in mesh = %zu",
                        elementIndex, blockMesh->faces.size());
            }

            elementIndex++;
        }

        LogInfo(LogBlockModelCompiler, "[Compile] Generated block mesh with %zu faces", blockMesh->faces.size());

        if (isDebugModel && blockMesh->faces.empty())
        {
            LogError(LogBlockModelCompiler, "[DEBUG STAIRS] ERROR: Generated 0 faces for stairs model!");
            ERROR_RECOVERABLE("Stairs model generated 0 faces!");
        }

        LogInfo(LogBlockModelCompiler, "[Compile] ========== Complete for: %s ==========", modelName.c_str());

        return blockMesh;
    }

    void BlockModelCompiler::CompileElementToFaces(const resource::model::ModelElement&            element,
                                                   std::shared_ptr<resource::model::ModelResource> model,
                                                   std::shared_ptr<BlockRenderMesh>                blockMesh)
    {
        if (m_context.enableLogging)
        {
            LogInfo(LogBlockModelCompiler, "Compiling element with %zu faces (from: %.1f,%.1f,%.1f, to: %.1f,%.1f,%.1f)",
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
            // [FIX] Use ModelResource::ResolveTexture() for Minecraft-style chain resolution
            // This handles "#particle" -> "#side" -> "minecraft:block/stone" chains
            ResourceLocation textureLocation = model->ResolveTexture(modelFace.texture);

            // [FIX] Get base atlas UV for the full texture
            auto [atlasUvMin, atlasUvMax] = GetAtlasUV(textureLocation);

            // [FIX] Apply modelFace.uv to get the sub-region within the texture
            // modelFace.uv is in Minecraft texture space (0-16 pixels)
            // Convert to 0-1 normalized, then map to atlas coordinates
            float faceUvMinX = modelFace.uv.x / 16.0f;
            float faceUvMinY = modelFace.uv.y / 16.0f;
            float faceUvMaxX = modelFace.uv.z / 16.0f;
            float faceUvMaxY = modelFace.uv.w / 16.0f;

            // Interpolate within atlas sprite: atlasUV = atlasMin + (atlasMax - atlasMin) * faceUV
            Vec2 uvMin(
                atlasUvMin.x + (atlasUvMax.x - atlasUvMin.x) * faceUvMinX,
                atlasUvMin.y + (atlasUvMax.y - atlasUvMin.y) * faceUvMinY
            );
            Vec2 uvMax(
                atlasUvMin.x + (atlasUvMax.x - atlasUvMin.x) * faceUvMaxX,
                atlasUvMin.y + (atlasUvMax.y - atlasUvMin.y) * faceUvMaxY
            );

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
        // [IMPORTANT] Coordinate system conversion:
        //   Minecraft: +X=East, +Y=Up, +Z=South
        //   SimpleMiner: +X=Forward, +Y=Left, +Z=Up
        //   Mapping: SimpleMiner(x,y,z) = Minecraft(x,z,y)
        Vec3 mcFrom = element.from / 16.0f;
        Vec3 mcTo   = element.to / 16.0f;

        // [FIX] Apply Y<->Z swap for SimpleMiner coordinate system
        Vec3 from(mcFrom.x, mcFrom.z, mcFrom.y); // (x, y, z) = (x, z, y)
        Vec3 to(mcTo.x, mcTo.z, mcTo.y); // (x, y, z) = (x, z, y)

        // Create quad vertices based on direction and element bounds
        // SimpleMiner coordinate system: +X=Forward, +Y=Left, +Z=Up
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
            LogWarn(LogBlockModelCompiler, "No atlas available for texture: %s", textureLocation.ToString().c_str());
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
            LogInfo(LogBlockModelCompiler, "Looking for texture: %s -> %s in atlas with %zu sprites", textureLocation.ToString().c_str(), actualTextureLocation.ToString().c_str(),
                    m_atlas->GetAllSprites().size());
        }

        // Find sprite in atlas using the resolved path
        const auto* sprite = m_atlas->FindSprite(actualTextureLocation);
        if (!sprite)
        {
            if (m_context.enableLogging)
            {
                LogWarn(LogBlockModelCompiler, "Texture not found in atlas: %s (resolved to %s)", textureLocation.ToString().c_str(), actualTextureLocation.ToString().c_str());

                // List all sprites in atlas for debugging
                const auto& allSprites = m_atlas->GetAllSprites();
                for (const auto& spriteInfo : allSprites)
                {
                    LogInfo(LogBlockModelCompiler, "  Atlas contains sprite: %s", spriteInfo.location.ToString().c_str());
                }
            }
            return {Vec2(0, 0), Vec2(1, 1)};
        }

        if (m_context.enableLogging)
        {
            LogInfo(LogBlockModelCompiler, "Found texture: %s -> %s", textureLocation.ToString().c_str(), actualTextureLocation.ToString().c_str());
        }

        return {sprite->uvMin, sprite->uvMax};
    }
}
