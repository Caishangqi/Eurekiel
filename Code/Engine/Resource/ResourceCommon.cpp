#include "ResourceCommon.hpp"

#include <regex>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <locale>

#include "Engine/Core/StringUtils.hpp"

using namespace enigma::resource;

std::string GetFileExtension(const std::string& filePath)
{
    Strings split = SplitStringOnDelimiter(filePath, '.');
    if ((int)split.size() > 1)
    {
        return split.back();
    }
    return "";
}

// Main constructor with validation
ResourceLocation::ResourceLocation(std::string_view namespaceId, std::string_view path)
    : m_namespace(namespaceId), m_path(path)
{
    validateAndNormalize();
}

// Parse from string format "namespace:path"
ResourceLocation::ResourceLocation(std::string_view fullLocation)
{
    auto parsed = TryParse(fullLocation);
    if (!parsed)
        throw std::invalid_argument("Invalid resource location: " + std::string(fullLocation));
    *this = *parsed;
}

// Static factory methods (Neoforge style)
ResourceLocation ResourceLocation::Of(std::string_view namespaceId, std::string_view path)
{
    return ResourceLocation(namespaceId, path);
}

ResourceLocation ResourceLocation::Parse(std::string_view location)
{
    return ResourceLocation(location);
}

ResourceLocation ResourceLocation::FromNamespaceAndPath(std::string_view namespaceId, std::string_view path)
{
    return ResourceLocation(namespaceId, path);
}

// Tryparse version that returns optional instead of throwing
std::optional<ResourceLocation> ResourceLocation::TryParse(std::string_view location)
{
    if (location.empty()) return std::nullopt;

    size_t colonPos = location.find(':');
    if (colonPos == std::string_view::npos)
    {
        // No namespace specified, use default
        if (!IsValidPath(location))
            return std::nullopt;

        ResourceLocation result;
        result.m_namespace = DEFAULT_NAMESPACE;
        result.m_path      = location;
        return result;
    }

    std::string_view namespaceId = location.substr(0, colonPos);
    std::string_view path        = location.substr(colonPos + 1);

    if (!IsValidNamespace(namespaceId) || !IsValidPath(path))
        return std::nullopt;

    ResourceLocation result;
    result.m_namespace = namespaceId;
    result.m_path      = path;
    return result;
}

// Create with default namespace
ResourceLocation ResourceLocation::WithDefaultNamespace(std::string_view path)
{
    return ResourceLocation(DEFAULT_NAMESPACE, path);
}

// Utility methods
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

ResourceLocation ResourceLocation::WithNamespace(std::string_view newNamespace) const
{
    return ResourceLocation(newNamespace, m_path);
}

// Validation
bool ResourceLocation::IsValid() const
{
    return !m_namespace.empty() && !m_path.empty() &&
        IsValidNamespace(m_namespace) && IsValidPath(m_path);
}

bool ResourceLocation::IsValidNamespace(std::string_view namespaceId)
{
    if (namespaceId.empty()) return false;

    // Neoforge standard: namespace can contain a-z, 0-9, _, ., -
    for (char c : namespaceId)
    {
        if (!std::islower(c) && !std::isdigit(c) && c != '_' && c != '.' && c != '-')
            return false;
    }
    return true;
}

bool ResourceLocation::IsValidPath(std::string_view path)
{
    if (path.empty()) return false;

    // Neoforge standard: path can contain a-z, 0-9, _, ., -, /
    for (char c : path)
    {
        if (!std::islower(c) && !std::isdigit(c) && c != '_' && c != '.' && c != '-' && c != '/')
            return false;
    }

    // Path cannot start or end with '/', and cannot have consecutive '/'
    if (path.front() == '/' || path.back() == '/')
        return false;

    // Check for consecutive slashes
    for (size_t i = 0; i < path.length() - 1; ++i)
    {
        if (path[i] == '/' && path[i + 1] == '/')
            return false;
    }

    return true;
}

// Comparison operators
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

// Comparison with string (Neoforge convenience)
bool ResourceLocation::operator==(std::string_view str) const
{
    return ToString() == str;
}

bool ResourceLocation::operator!=(std::string_view str) const
{
    return ToString() != str;
}

// Private validation and normalization
void ResourceLocation::validateAndNormalize()
{
    // Convert to lowercase (Neoforge requirement)
    std::transform(m_namespace.begin(), m_namespace.end(), m_namespace.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(m_path.begin(), m_path.end(), m_path.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (!IsValidNamespace(m_namespace))
        throw std::invalid_argument("Invalid namespace: " + m_namespace);
    if (!IsValidPath(m_path))
        throw std::invalid_argument("Invalid path: " + m_path);
}
