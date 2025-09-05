#include "ModelResource.hpp"
#include "../../Core/Engine.hpp"
#include "../ResourceSubsystem.hpp"

using namespace enigma::resource::model;

ModelFace::ModelFace(const std::string& tex) : texture(tex)
{
}

bool ModelFace::LoadFromJson(const JsonObject& json)
{
    if (json.Has("texture"))
    {
        texture = json.GetString("texture");
    }

    if (json.Has("uv"))
    {
        // Use std::vector<JsonObject> from GetJsonArray
        auto uvArray = json.GetJsonArray("uv");
        if (uvArray.size() >= 4)
        {
            uv.x = (float)uvArray[0].GetJson().get<int>();
            uv.y = (float)uvArray[1].GetJson().get<int>();
            uv.z = (float)uvArray[2].GetJson().get<int>();
            uv.w = (float)uvArray[3].GetJson().get<int>();
        }
    }

    if (json.Has("rotation"))
    {
        rotation = json.GetInt("rotation");
    }

    if (json.Has("cullface"))
    {
        cullFace = json.GetString("cullface");
    }

    if (json.Has("tintindex"))
    {
        tintIndex = json.GetInt("tintindex") != 0;
    }

    return true;
}

bool ModelElement::LoadFromJson(const JsonObject& json)
{
    if (json.Has("from"))
    {
        auto fromArray = json.GetJsonArray("from");
        if (fromArray.size() >= 3)
        {
            from.x = (float)fromArray[0].GetJson().get<int>();
            from.y = (float)fromArray[1].GetJson().get<int>();
            from.z = (float)fromArray[2].GetJson().get<int>();
        }
    }

    if (json.Has("to"))
    {
        auto toArray = json.GetJsonArray("to");
        if (toArray.size() >= 3)
        {
            to.x = (float)toArray[0].GetJson().get<int>();
            to.y = (float)toArray[1].GetJson().get<int>();
            to.z = (float)toArray[2].GetJson().get<int>();
        }
    }

    if (json.Has("faces"))
    {
        auto                           facesObj       = json.GetJsonObject("faces");
        const std::vector<std::string> faceDirections = {
            "down", "up", "north", "south", "east", "west"
        };

        for (const auto& dir : faceDirections)
        {
            if (facesObj.Has(dir))
            {
                ModelFace face;
                auto      faceObj = facesObj.GetJsonObject(dir);
                if (face.LoadFromJson(faceObj))
                {
                    faces[dir] = face;
                }
            }
        }
    }

    if (json.Has("rotation"))
    {
        Rotation rot;
        auto     rotObj = json.GetJsonObject("rotation");
        if (rot.LoadFromJson(rotObj))
        {
            rotation = rot;
        }
    }

    if (json.Has("shade"))
    {
        shade = json.GetBool("shade");
    }

    return true;
}

bool ModelElement::Rotation::LoadFromJson(const JsonObject& json)
{
    if (json.Has("origin"))
    {
        auto originArray = json.GetJsonArray("origin");
        if (originArray.size() >= 3)
        {
            origin.x = (float)originArray[0].GetJson().get<int>();
            origin.y = (float)originArray[1].GetJson().get<int>();
            origin.z = (float)originArray[2].GetJson().get<int>();
        }
    }

    if (json.Has("axis"))
    {
        axis = json.GetString("axis");
    }

    if (json.Has("angle"))
    {
        angle = (float)json.GetInt("angle");
    }

    if (json.Has("rescale"))
    {
        rescale = json.GetBool("rescale");
    }

    return true;
}

bool ModelDisplay::LoadFromJson(const JsonObject& json)
{
    if (json.Has("rotation"))
    {
        auto rotArray = json.GetJsonArray("rotation");
        if (rotArray.size() >= 3)
        {
            rotation.x = (float)rotArray[0].GetJson().get<int>();
            rotation.y = (float)rotArray[1].GetJson().get<int>();
            rotation.z = (float)rotArray[2].GetJson().get<int>();
        }
    }

    if (json.Has("translation"))
    {
        auto transArray = json.GetJsonArray("translation");
        if (transArray.size() >= 3)
        {
            translation.x = (float)transArray[0].GetJson().get<int>();
            translation.y = (float)transArray[1].GetJson().get<int>();
            translation.z = (float)transArray[2].GetJson().get<int>();
        }
    }

    if (json.Has("scale"))
    {
        auto scaleArray = json.GetJsonArray("scale");
        if (scaleArray.size() >= 3)
        {
            scale.x = (float)scaleArray[0].GetJson().get<int>();
            scale.y = (float)scaleArray[1].GetJson().get<int>();
            scale.z = (float)scaleArray[2].GetJson().get<int>();
        }
    }

    return true;
}

ModelResource::ModelResource(const ResourceLocation& location)
{
    m_metadata.location = location;
    m_metadata.type     = ResourceType::MODEL;
    m_metadata.state    = ResourceState::NOT_LOADED;
}

bool ModelResource::LoadFromJson(const JsonObject& json)
{
    // Parse parent
    if (json.Has("parent"))
    {
        std::string parent = json.GetString("parent");
        if (!parent.empty())
        {
            m_parent = ResourceLocation::Parse(parent);
        }
    }

    // Parse textures
    if (json.Has("textures"))
    {
        const auto texturesObj = json.GetJsonObject("textures");
        // Note: We need to iterate through the texture object
        // For now, let's manually parse known texture keys
        const std::vector<std::string> textureKeys = {
            "particle", "down", "up", "north", "south", "east", "west", "all"
        };

        for (const auto& key : textureKeys)
        {
            if (texturesObj.Has(key))
            {
                m_textures[key] = ResourceLocation::Parse(texturesObj.GetString(key));
            }
        }
    }

    // Parse elements (for complex models)
    if (json.Has("elements"))
    {
        const auto elementsArray = json.GetJsonArray("elements");
        for (size_t i = 0; i < elementsArray.size(); ++i)
        {
            ModelElement element;
            const auto&  elementJson = elementsArray[i]; // This is JsonObject
            if (element.LoadFromJson(elementJson))
            {
                m_elements.push_back(element);
            }
        }
    }

    // Parse display settings
    if (json.Has("display"))
    {
        const auto displayObj = json.GetJsonObject("display");
        // Note: Similar to textures, we need to iterate manually
        const std::vector<std::string> displayContexts = {
            "gui", "ground", "fixed", "thirdperson_righthand", "thirdperson_lefthand",
            "firstperson_righthand", "firstperson_lefthand", "head"
        };

        for (const auto& context : displayContexts)
        {
            if (displayObj.Has(context))
            {
                ModelDisplay display;
                const auto   displayValue = displayObj.GetJsonObject(context);
                if (display.LoadFromJson(displayValue))
                {
                    m_display[context] = display;
                }
            }
        }
    }

    // Parse ambient occlusion
    if (json.Has("ambientocclusion"))
    {
        m_ambientOcclusion = json.GetBool("ambientocclusion");
    }

    m_metadata.state = ResourceState::LOADED;
    return true;
}

const std::map<std::string, ResourceLocation>& ModelResource::GetResolvedTextures() const
{
    if (!m_resolved)
    {
        ResolveInheritance();
    }
    return m_resolvedTextures;
}

const std::vector<ModelElement>& ModelResource::GetResolvedElements() const
{
    if (!m_resolved)
    {
        ResolveInheritance();
    }
    return m_resolvedElements;
}

ResourceLocation ModelResource::ResolveTexture(const std::string& textureVariable) const
{
    const auto& textures = GetResolvedTextures();
    auto        it       = textures.find(textureVariable);
    return it != textures.end() ? it->second : ResourceLocation("minecraft", "missingno");
}

void ModelResource::ResolveInheritance() const
{
    if (m_resolved) return;

    // Start with our own textures and elements
    m_resolvedTextures = m_textures;
    m_resolvedElements = m_elements;

    // Resolve parent model if exists
    if (m_parent.has_value() && GEngine)
    {
        auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
        if (resourceSubsystem)
        {
            auto parentResource = resourceSubsystem->GetResource(m_parent.value());
            auto parentModel    = std::dynamic_pointer_cast<ModelResource>(parentResource);
            if (parentModel)
            {
                ResolveParentRecursive(parentModel.get());
            }
        }
    }

    m_resolved = true;
}

void ModelResource::ResolveParentRecursive(const ModelResource* parentModel) const
{
    if (!parentModel) return;

    // Copy parent textures (don't override existing ones)
    for (const auto& [key, texture] : parentModel->m_textures)
    {
        if (m_resolvedTextures.find(key) == m_resolvedTextures.end())
        {
            m_resolvedTextures[key] = texture;
        }
    }

    // Copy parent elements if we don't have any
    if (m_resolvedElements.empty())
    {
        m_resolvedElements = parentModel->m_elements;
    }
}

const ModelDisplay* ModelResource::GetDisplay(const std::string& context) const
{
    auto it = m_display.find(context);
    return it != m_display.end() ? &it->second : nullptr;
}
