#pragma once

#include "ThirdParty/TineyXML2/tinyxml2.h"

#include <string>
#include "StringUtils.hpp"

struct Vec3;
struct IntVec2;
struct Vec2;
struct Rgba8;
struct FloatRange;
using XmlDocument  = tinyxml2::XMLDocument;
using XmlElement   = tinyxml2::XMLElement;
using XmlAttribute = tinyxml2::XMLAttribute;
using XmlResult    = tinyxml2::XMLError;

int         ParseXmlAttribute(const XmlElement& element, const char* attributeName, int defaultValue);
char        ParseXmlAttribute(const XmlElement& element, const char* attributeName, char defaultValue);
bool        ParseXmlAttribute(const XmlElement& element, const char* attributeName, bool defaultValue);
float       ParseXmlAttribute(const XmlElement& element, const char* attributeName, float defaultValue);
Rgba8       ParseXmlAttribute(const XmlElement& element, const char* attributeName, const Rgba8& defaultValue);
Vec2        ParseXmlAttribute(const XmlElement& element, const char* attributeName, const Vec2& defaultValue);
Vec3        ParseXmlAttribute(const XmlElement& element, const char* attributeName, const Vec3& defaultValue);
IntVec2     ParseXmlAttribute(const XmlElement& element, const char* attributeName, const IntVec2& defaultValue);
std::string ParseXmlAttribute(const XmlElement& element, const char* attributeName, const std::string& defaultValue);
Strings     ParseXmlAttribute(const XmlElement& element, const char* attributeName, const Strings& defaultValue);
FloatRange  ParseXmlAttribute(const XmlElement& element, const char* attributeName, const FloatRange& defaultValue);

const XmlElement* FindChildElementByName(const XmlElement& element, const char* childElementName);
