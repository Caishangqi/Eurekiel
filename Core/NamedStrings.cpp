#include "NamedStrings.hpp"

#include "Rgba8.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec2.hpp"

void NamedStrings::PopulateFromXmlElementAttributes(const XmlElement& element)
{
    const XmlAttribute* attribute = element.FirstAttribute();
    while (attribute)
    {
        SetValue(attribute->Name(), attribute->Value()); // put XML attribute name and value is as (key, value) pair
        attribute = attribute->Next();
    }
}

void NamedStrings::SetValue(const std::string& keyName, const std::string& newValue)
{
    m_keyValuePairs[keyName] = newValue;
}

std::string NamedStrings::GetValue(const std::string& keyName, const std::string& defaultValue) const
{
    auto iter = m_keyValuePairs.find(keyName);
    if (iter != m_keyValuePairs.end())
    {
        return iter->second;
    }
    return defaultValue;
}

bool NamedStrings::GetValue(const std::string& keyName, bool defaultValue) const
{
    auto iter = m_keyValuePairs.find(keyName);
    if (iter != m_keyValuePairs.end())
    {
        return iter->second == "true";
    }
    return defaultValue;
}

int NamedStrings::GetValue(const std::string& keyName, int defaultValue) const
{
    auto iter = m_keyValuePairs.find(keyName);
    if (iter != m_keyValuePairs.end())
    {
        return std::stoi(iter->second);
    }
    return defaultValue;
}

float NamedStrings::GetValue(const std::string& keyName, float defaultValue) const
{
    auto iter = m_keyValuePairs.find(keyName);
    if (iter != m_keyValuePairs.end())
    {
        return std::stof(iter->second);
    }
    return defaultValue;
}

std::string NamedStrings::GetValue(const std::string& keyName, const char* defaultValue) const
{
    return GetValue(keyName, std::string(defaultValue));
}

Rgba8 NamedStrings::GetValue(const std::string& keyName, const Rgba8& defaultValue) const
{
    auto iter = m_keyValuePairs.find(keyName);
    if (iter != m_keyValuePairs.end())
    {
        Rgba8 result = defaultValue;
        result.SetFromText(iter->second.c_str()); // 假设 Rgba8 有一个类似的方法来从字符串解析
        return result;
    }
    return defaultValue;
}

Vec2 NamedStrings::GetValue(const std::string& keyName, const Vec2& defaultValue) const
{
    auto iter = m_keyValuePairs.find(keyName);
    if (iter != m_keyValuePairs.end())
    {
        Vec2 result = defaultValue;
        result.SetFromText(iter->second.c_str());
        return result;
    }
    return defaultValue;
}

IntVec2 NamedStrings::GetValue(const std::string& keyName, const IntVec2& defaultValue) const
{
    auto iter = m_keyValuePairs.find(keyName);
    if (iter != m_keyValuePairs.end())
    {
        IntVec2 result = defaultValue;
        result.SetFromText(iter->second.c_str());
        return result;
    }
    return defaultValue;
}
