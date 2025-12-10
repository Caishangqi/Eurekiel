#include "ModelResource.hpp"
#include "../../Core/Engine.hpp"
#include "../ResourceSubsystem.hpp"
#include "../../Core/Logger/LoggerAPI.hpp"
#include "../../Core/ErrorWarningAssert.hpp"
#include <unordered_set>

using namespace enigma::resource::model;
using namespace enigma::core;

DEFINE_LOG_CATEGORY(LogModelResource)

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
    // [Minecraft Convention] JSON uses "block/xxx" but files are at "models/block/xxx"
    // We need to add "models/" prefix to match the scanned ResourceLocation
    if (json.Has("parent"))
    {
        std::string parent         = json.GetString("parent");
        std::string originalParent = parent; // Save for logging
        if (!parent.empty())
        {
            // Add "models/" prefix if the path doesn't already have it
            // This converts "block/slab" -> "models/block/slab"
            if (parent.find("models/") != 0 && parent.find(':') == std::string::npos)
            {
                // No namespace prefix and no "models/" prefix - add it
                parent = "models/" + parent;
            }
            else if (parent.find(':') != std::string::npos)
            {
                // Has namespace prefix like "minecraft:block/slab" -> "minecraft:models/block/slab"
                size_t      colonPos = parent.find(':');
                std::string ns       = parent.substr(0, colonPos + 1);
                std::string path     = parent.substr(colonPos + 1);
                if (path.find("models/") != 0)
                {
                    parent = ns + "models/" + path;
                }
            }
            m_parent = ResourceLocation::Parse(parent);
            LogInfo(LogModelResource, "[LoadFromJson] Model %s: parent '%s' -> '%s'",
                    m_metadata.location.ToString().c_str(), originalParent.c_str(), m_parent.value().ToString().c_str());
        }
    }

    // Parse textures following Minecraft's Either<Material, String> design
    // JSON values can be:
    //   1. ResourceLocation strings like "minecraft:block/stone" -> TextureEntry(ResourceLocation)
    //   2. Variable references like "#side" (starts with '#') -> TextureEntry(std::string) without '#'
    if (json.Has("textures"))
    {
        const auto texturesObj = json.GetJsonObject("textures");
        // Known texture variable keys used in Minecraft models
        const std::vector<std::string> textureKeys = {
            "particle", "down", "up", "north", "south", "east", "west", "all",
            "bottom", "top", "side", "front", "back", "end", "cross", "plant"
        };

        for (const auto& key : textureKeys)
        {
            if (texturesObj.Has(key))
            {
                std::string value = texturesObj.GetString(key);

                // [Minecraft Design] Distinguish between variable reference and texture location
                if (!value.empty() && value[0] == '#')
                {
                    // Variable reference: "#side" -> store "side" (without '#' prefix)
                    m_textures[key] = value.substr(1); // TextureEntry as std::string (variant index 1)
                }
                else
                {
                    // Texture location: "minecraft:block/stone" -> parse to ResourceLocation
                    m_textures[key] = ResourceLocation::Parse(value); // TextureEntry as ResourceLocation (variant index 0)
                }
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

const std::map<std::string, TextureEntry>& ModelResource::GetResolvedTextures() const
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

/**
 * @brief Resolves a texture variable to a ResourceLocation following Minecraft's Either<Material, String> design
 *
 * Minecraft texture resolution algorithm (using std::variant):
 * 1. Remove '#' prefix if present to get the variable name
 * 2. Loop: Look up the variable in the resolved textures map
 *    - If TextureEntry holds ResourceLocation (index 0): Return it directly
 *    - If TextureEntry holds std::string (index 1): Continue chain resolution
 * 3. Detect circular references to prevent infinite loops
 * 4. Return missing texture if not found
 *
 * Example chain: "#particle" -> "side" -> ResourceLocation("minecraft:block/stone")
 */
ResourceLocation ModelResource::ResolveTexture(const std::string& textureVariable) const
{
    // Ensure inheritance is resolved
    if (!m_resolved)
    {
        ResolveInheritance();
    }

    // [STEP1] Remove '#' prefix if present (for compatibility with face texture references)
    std::string varName = textureVariable;
    if (!varName.empty() && varName[0] == '#')
    {
        varName = varName.substr(1);
    }

    // [STEP2] Chain resolution with cycle detection (Minecraft design)
    std::unordered_set<std::string> visited;

    while (true)
    {
        auto it = m_resolvedTextures.find(varName);
        if (it == m_resolvedTextures.end())
        {
            // [NOT_FOUND] Variable not defined, return missing texture
            return ResourceLocation("minecraft", "missingno");
        }

        const TextureEntry& entry = it->second;

        // [Minecraft Either Design] Check variant type
        if (IsTextureLocation(entry))
        {
            // [FINAL] TextureEntry holds ResourceLocation (Either.left) - return directly
            return GetTextureLocation(entry);
        }
        else
        {
            // [CHAIN] TextureEntry holds std::string (Either.right) - continue chain
            const std::string& nextVar = GetVariableReference(entry);

            // [CYCLE_CHECK] Detect circular references
            if (visited.count(nextVar) > 0)
            {
                // [WARNING] Circular reference detected
                return ResourceLocation("minecraft", "missingno");
            }
            visited.insert(varName);
            varName = nextVar;
        }
    }
}

void ModelResource::ResolveInheritance() const
{
    if (m_resolved) return;

    std::string modelName    = m_metadata.location.ToString();
    bool        isDebugModel = (modelName.find("stairs") != std::string::npos);

    LogInfo(LogModelResource, "[ResolveInheritance] Starting for model: %s", modelName.c_str());
    LogInfo(LogModelResource, "  Own elements: %zu, Own textures: %zu", m_elements.size(), m_textures.size());

    if (isDebugModel)
    {
        LogInfo(LogModelResource, "[DEBUG STAIRS] ResolveInheritance for: %s", modelName.c_str());
        LogInfo(LogModelResource, "[DEBUG STAIRS] Own elements: %zu", m_elements.size());
        for (size_t i = 0; i < m_elements.size(); ++i)
        {
            LogInfo(LogModelResource, "[DEBUG STAIRS]   Element[%zu]: %zu faces", i, m_elements[i].faces.size());
        }
    }

    // Start with our own textures and elements
    m_resolvedTextures = m_textures;
    m_resolvedElements = m_elements;

    // Resolve parent model if exists
    if (m_parent.has_value() && GEngine)
    {
        LogInfo(LogModelResource, "  Has parent: %s", m_parent.value().ToString().c_str());

        auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
        if (resourceSubsystem)
        {
            auto parentResource = resourceSubsystem->GetResource(m_parent.value());
            if (!parentResource)
            {
                // [CRITICAL] Parent model not found - this is the likely cause of missing geometry
                LogError(LogModelResource, "[ERROR] Parent model NOT FOUND: %s", m_parent.value().ToString().c_str());
                LogError(LogModelResource, "  Model %s will have NO GEOMETRY because parent is missing!", m_metadata.location.ToString().c_str());
                ERROR_RECOVERABLE(Stringf("Parent model not found: %s (for model %s)",
                    m_parent.value().ToString().c_str(), m_metadata.location.ToString().c_str()));
            }
            else
            {
                auto parentModel = std::dynamic_pointer_cast<ModelResource>(parentResource);
                if (parentModel)
                {
                    LogInfo(LogModelResource, "  Parent model loaded successfully, elements: %zu", parentModel->m_elements.size());
                    ResolveParentRecursive(parentModel.get());
                }
                else
                {
                    LogError(LogModelResource, "[ERROR] Parent resource is not a ModelResource: %s", m_parent.value().ToString().c_str());
                    ERROR_RECOVERABLE(Stringf("Parent is not ModelResource: %s", m_parent.value().ToString().c_str()));
                }
            }
        }
        else
        {
            LogError(LogModelResource, "[ERROR] ResourceSubsystem not available!");
        }
    }
    else if (!m_parent.has_value())
    {
        LogInfo(LogModelResource, "  No parent model");
    }

    LogInfo(LogModelResource, "  Final resolved elements: %zu, textures: %zu", m_resolvedElements.size(), m_resolvedTextures.size());

    if (isDebugModel)
    {
        LogInfo(LogModelResource, "[DEBUG STAIRS] Final resolved elements: %zu", m_resolvedElements.size());
        for (size_t i = 0; i < m_resolvedElements.size(); ++i)
        {
            LogInfo(LogModelResource, "[DEBUG STAIRS]   ResolvedElement[%zu]: %zu faces", i, m_resolvedElements[i].faces.size());
        }
    }

    m_resolved = true;
}

/**
 * @brief Recursively resolve parent model chain following Minecraft's inheritance design
 *
 * Minecraft parent chain resolution:
 * 1. Traverse from child to parent (deepest parent first)
 * 2. Copy textures from parent if not defined in child (child overrides parent)
 * 3. Copy elements from parent if child has none
 *
 * Example: stone.json -> cube_all.json -> cube.json -> block.json
 */
void ModelResource::ResolveParentRecursive(const ModelResource* parentModel) const
{
    if (!parentModel) return;

    LogInfo(LogModelResource, "  [ResolveParentRecursive] Processing parent: %s (elements: %zu, textures: %zu)",
            parentModel->m_metadata.location.ToString().c_str(), parentModel->m_elements.size(), parentModel->m_textures.size());

    // [STEP1] First, recursively resolve the parent's parent (if any)
    // This ensures we process the chain from deepest parent to immediate parent
    if (parentModel->m_parent.has_value() && GEngine)
    {
        LogInfo(LogModelResource, "    Parent has grandparent: %s", parentModel->m_parent.value().ToString().c_str());

        auto* resourceSubsystem = GEngine->GetSubsystem<enigma::resource::ResourceSubsystem>();
        if (resourceSubsystem)
        {
            auto grandparentResource = resourceSubsystem->GetResource(parentModel->m_parent.value());
            if (!grandparentResource)
            {
                LogError(LogModelResource, "    [ERROR] Grandparent NOT FOUND: %s", parentModel->m_parent.value().ToString().c_str());
                ERROR_RECOVERABLE(Stringf("Grandparent model not found: %s", parentModel->m_parent.value().ToString().c_str()));
            }
            else
            {
                auto grandparentModel = std::dynamic_pointer_cast<ModelResource>(grandparentResource);
                if (grandparentModel)
                {
                    ResolveParentRecursive(grandparentModel.get());
                }
                else
                {
                    LogError(LogModelResource, "    [ERROR] Grandparent is not ModelResource: %s", parentModel->m_parent.value().ToString().c_str());
                }
            }
        }
    }

    // [STEP2] Copy parent textures (don't override existing ones - child overrides parent)
    for (const auto& [key, texture] : parentModel->m_textures)
    {
        if (m_resolvedTextures.find(key) == m_resolvedTextures.end())
        {
            m_resolvedTextures[key] = texture;
        }
    }

    // [STEP3] Copy parent elements if we don't have any
    if (m_resolvedElements.empty())
    {
        LogInfo(LogModelResource, "    Copying %zu elements from parent (child had none)", parentModel->m_elements.size());
        m_resolvedElements = parentModel->m_elements;
    }
    else
    {
        LogInfo(LogModelResource, "    Keeping child's %zu elements (not copying from parent)", m_resolvedElements.size());
    }
}

const ModelDisplay* ModelResource::GetDisplay(const std::string& context) const
{
    auto it = m_display.find(context);
    return it != m_display.end() ? &it->second : nullptr;
}
