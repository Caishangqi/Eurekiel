#include "ModelResource.hpp"

using namespace enigma::resource::model;

ModelFace::ModelFace(const std::string& tex) : texture(tex)
{
}

ModelResource::ModelResource(const ResourceLocation& location)
{
    m_metadata.location = location;
    m_metadata.type     = ResourceType::MODEL;
    m_metadata.state    = ResourceState::NOT_LOADED;
}

bool ModelResource::LoadFromJson(const JsonObject& json)
{
    UNUSED(json)
    return true;
}

const ModelDisplay* ModelResource::GetDisplay(const std::string& context) const
{
    auto it = m_display.find(context);
    return it != m_display.end() ? &it->second : nullptr;
}
