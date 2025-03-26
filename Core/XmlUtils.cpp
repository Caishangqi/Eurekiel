#include "XmlUtils.hpp"

#include "EngineCommon.hpp"
#include "ErrorWarningAssert.hpp"
#include "Rgba8.hpp"
#include "Engine/Math/FloatRange.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"

int ParseXmlAttribute(const XmlElement& element, const char* attributeName, int defaultValue)
{
    int result = defaultValue;
    element.QueryIntAttribute(attributeName, &result);
    return result;
}

char ParseXmlAttribute(const XmlElement& element, const char* attributeName, char defaultValue)
{
    return static_cast<char>(element.UnsignedAttribute(attributeName, defaultValue));
}

bool ParseXmlAttribute(const XmlElement& element, const char* attributeName, bool defaultValue)
{
    return element.BoolAttribute(attributeName, defaultValue);
}

float ParseXmlAttribute(const XmlElement& element, const char* attributeName, float defaultValue)
{
    return element.FloatAttribute(attributeName, defaultValue);
}

Rgba8 ParseXmlAttribute(const XmlElement& element, const char* attributeName, const Rgba8& defaultValue)
{
    Rgba8 result = defaultValue;
    result.SetFromText(element.Attribute(attributeName));
    return result;
}

Vec2 ParseXmlAttribute(const XmlElement& element, const char* attributeName, const Vec2& defaultValue)
{
    Vec2 result = defaultValue;
    result.SetFromText(element.Attribute(attributeName));
    return result;
}

Vec3 ParseXmlAttribute(const XmlElement& element, const char* attributeName, const Vec3& defaultValue)
{
    Vec3 result = defaultValue;
    result.SetFromText(element.Attribute(attributeName));
    return result;
}

IntVec2 ParseXmlAttribute(const XmlElement& element, const char* attributeName, const IntVec2& defaultValue)
{
    IntVec2 result = defaultValue;
    result.SetFromText(element.Attribute(attributeName));
    return result;
}

std::string ParseXmlAttribute(const XmlElement& element, const char* attributeName, const std::string& defaultValue)
{
    const char* attribute = element.Attribute(attributeName);
    if (attribute == nullptr)
    {
        return defaultValue; // 返回默认值
    }
    return std::string(attribute); // 转换为 std::string 返回
}


Strings ParseXmlAttribute(const XmlElement& element, const char* attributeName, const Strings& defaultValue)
{
    UNUSED(element)
    UNUSED(attributeName)
    UNUSED(defaultValue)
    ERROR_AND_DIE("ParseXmlAttribute not implemented")
}

FloatRange ParseXmlAttribute(const XmlElement& element, const char* attributeName, const FloatRange& defaultValue)
{
    FloatRange result = defaultValue;
    result.SetFromText(element.Attribute(attributeName));
    return result;
}

const XmlElement* FindChildElementByName(const XmlElement& element, const char* childElementName)
{
    int childElementCount = element.ChildElementCount();
    if (childElementCount <= 0)
        return nullptr;
    const XmlElement* firstChildElement = element.FirstChildElement();
    while (firstChildElement != nullptr)
    {
        if (firstChildElement->Name() == std::string(childElementName))
        {
            return firstChildElement;
        }
        firstChildElement = firstChildElement->NextSiblingElement();
    }
    return nullptr;
}
