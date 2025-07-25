#pragma once
#include <map>
#include <string>

#include "XmlUtils.hpp"

///
/// NamedString is like Unreal Engine 5 blackboard that text based
/// board that user could create keys and read and set data in run-time only
///
class NamedStrings
{
public:
    void        PopulateFromXmlElementAttributes(const XmlElement& element);
    void        SetValue(const std::string& keyName, const std::string& newValue);
    std::string GetValue(const std::string& keyName, const std::string& defaultValue) const;
    bool        GetValue(const std::string& keyName, bool defaultValue) const;
    int         GetValue(const std::string& keyName, int defaultValue) const;
    float       GetValue(const std::string& keyName, float defaultValue) const;
    std::string GetValue(const std::string& keyName, const char* defaultValue) const;
    Rgba8       GetValue(const std::string& keyName, const Rgba8& defaultValue) const;
    Vec2        GetValue(const std::string& keyName, const Vec2& defaultValue) const;
    IntVec2     GetValue(const std::string& keyName, const IntVec2& defaultValue) const;

private:
    std::map<std::string, std::string> m_keyValuePairs;
};
