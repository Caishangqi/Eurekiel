#include "ResourceCommon.hpp"

#include <regex>
#include <stdexcept>

#include "Engine/Core/StringUtils.hpp"

using namespace enigma::resource;

std::string GetFileExtension(const std::string& filePath)
{
    Strings split = SplitStringOnDelimiter(filePath, '.');
    if ((int)split.size() > 1)
    {
        return *split.end();
    }
    return "";
}

ResourceLocation::ResourceLocation(std::string_view namespace_id, std::string_view path) : m_namespace(namespace_id), m_path(path)
{
    if (!IsValidNamespace(namespace_id))
        throw std::invalid_argument("Invalid namespace: " + std::string(namespace_id));
    if (!IsValidPath(path))
        throw std::invalid_argument("Invalid path: " + std::string(path));
}

ResourceLocation::ResourceLocation(std::string_view fullLocation)
{
    auto parsed = Parse(fullLocation);
    if (!parsed)
        throw std::invalid_argument("Invalid resource location: " + std::string(fullLocation));
    *this = *parsed;
}

std::optional<ResourceLocation> ResourceLocation::Parse(std::string_view location)
{
    size_t colonPos = location.find(':');
    if (colonPos == std::string_view::npos)
        return ResourceLocation(DEFAULT_NAMESPACE, location);

    std::string_view namespace_id = location.substr(0, colonPos);
    std::string_view path         = location.substr(colonPos + 1);

    if (!IsValidNamespace(namespace_id) || !IsValidPath(path))
        return std::nullopt;

    return ResourceLocation(namespace_id, path);
}

ResourceLocation ResourceLocation::WithDefaultNamespace(std::string_view path)
{
    return ResourceLocation(DEFAULT_NAMESPACE, path);
}

std::string ResourceLocation::ToString() const
{
    return m_namespace + ":" + m_path;
}

ResourceLocation ResourceLocation::WithPrefix(std::string_view prefix) const
{
    return ResourceLocation(m_namespace, std::string(prefix) + m_path);
}

ResourceLocation ResourceLocation::WithSuffix(std::string_view suffix) const
{
    return ResourceLocation(m_namespace, m_path + std::string(suffix));
}

ResourceLocation ResourceLocation::WithPath(std::string_view newPath) const
{
    return ResourceLocation(m_namespace, newPath);
}

bool ResourceLocation::operator==(const ResourceLocation& other) const
{
    return m_namespace == other.m_namespace && m_path == other.m_path;
}

bool ResourceLocation::operator!=(const ResourceLocation& other) const
{
    return !(*this == other);
}

bool ResourceLocation::operator<(const ResourceLocation& other) const
{
    if (m_namespace != other.m_namespace)
        return m_namespace < other.m_namespace;
    return m_path < other.m_path;
}

bool ResourceLocation::IsValidNamespace(std::string_view namespace_id)
{
    if (namespace_id.empty()) return false;

    static const std::regex pattern("^[a-z0-9_.]+$");
    return std::regex_match(namespace_id.begin(), namespace_id.end(), pattern);
}

bool ResourceLocation::IsValidPath(std::string_view path)
{
    if (path.empty()) return false;
    static const std::regex pattern("^[a-z0-9_./-]+$");
    return std::regex_match(path.begin(), path.end(), pattern);
}
