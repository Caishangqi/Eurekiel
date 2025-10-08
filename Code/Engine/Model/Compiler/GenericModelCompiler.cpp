#include "Engine/Model/Compiler/GenericModelCompiler.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Renderer/Model/RenderMesh.hpp"

using namespace enigma::renderer::model;
using namespace enigma::voxel;

GenericModelCompiler::GenericModelCompiler()
{
}

std::shared_ptr<RenderMesh> GenericModelCompiler::Compile(std::shared_ptr<resource::model::ModelResource> model, const CompilerContext& context)
{
    SetCompilerContext(context);

    m_mesh = std::make_unique<RenderMesh>();
    UNUSED(model)
    // TODO: Implement the Model Compile
    return m_mesh;
}

void GenericModelCompiler::SetCompilerContext(const CompilerContext& context)
{
    m_context = context;

    // Set the atlas from the context
    if (context.blockAtlas)
    {
        // Convert const TextureAtlas* to shared_ptr
        // Note: This assumes the atlas lifetime is managed elsewhere
        m_atlas = std::shared_ptr<resource::TextureAtlas>(
            const_cast<resource::TextureAtlas*>(context.blockAtlas),
            [](resource::TextureAtlas*)
            {
            } // No-op deleter since we don't own the atlas
        );
    }
}

std::shared_ptr<enigma::resource::TextureAtlas> GenericModelCompiler::GetAtlas() const
{
    return m_atlas;
}

std::vector<RenderFace> GenericModelCompiler::CompileElements(const std::vector<resource::model::ModelElement>& elements, const std::map<std::string, resource::ResourceLocation>& resolvedTextures)
{
    UNUSED(elements)
    UNUSED(resolvedTextures)
    return {};
}

RenderFace GenericModelCompiler::CompileFace(const std::string&                                       faceDirection, const resource::model::ModelFace&, const resource::model::ModelElement& element,
                                             const std::map<std::string, resource::ResourceLocation>& resolvedTextures)
{
    UNUSED(faceDirection)
    UNUSED(element)
    UNUSED(resolvedTextures)
    return {};
}

Direction GenericModelCompiler::StringToDirection(const std::string& direction)
{
    if (direction == "down") return Direction::DOWN;
    if (direction == "up") return Direction::UP;
    if (direction == "north") return Direction::NORTH;
    if (direction == "south") return Direction::SOUTH;
    if (direction == "west") return Direction::WEST;
    if (direction == "east") return Direction::EAST;
    return Direction::NORTH; // Default
}

void GenericModelCompiler::SetAtlas(std::shared_ptr<resource::TextureAtlas> atlas)
{
    m_atlas = atlas;
}
