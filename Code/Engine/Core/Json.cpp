#include "Json.hpp"

using namespace enigma::core;

JsonObject::JsonObject() : m_json(Json::object())
{
}

JsonObject::JsonObject(const std::string& jsonString)
{
    try
    {
        m_json = Json::parse(jsonString);
    }
    catch (const Json::parse_error& e)
    {
        throw JsonException("Failed to parse JSON: " + std::string(e.what()));
    }
}

JsonObject::JsonObject(const Json& json) : m_json(json)
{
}

JsonObject::JsonObject(const JsonObject& other) : m_json(other.m_json)
{
}

JsonObject::JsonObject(JsonObject&& other) noexcept : m_json(std::move(other.m_json))
{
}

JsonObject& JsonObject::operator=(const JsonObject& other)
{
    if (this != &other)
    {
        m_json = other.m_json;
    }
    return *this;
}

JsonObject& JsonObject::operator=(JsonObject&& other) noexcept
{
    if (this != &other)
    {
        m_json = std::move(other.m_json);
    }
    return *this;
}

JsonObject JsonObject::Parse(const std::string& jsonString)
{
    return JsonObject(jsonString);
}

JsonObject JsonObject::Parse(const std::vector<uint8_t>& data)
{
    std::string jsonStr(data.begin(), data.end());
    return Parse(jsonStr);
}

std::optional<JsonObject> JsonObject::TryParse(const std::string& jsonString)
{
    try
    {
        return JsonObject(jsonString);
    }
    catch (const JsonException&)
    {
        return std::nullopt;
    }
}

bool JsonObject::IsNull() const { return m_json.is_null(); }
bool JsonObject::IsObject() const { return m_json.is_object(); }
bool JsonObject::IsArray() const { return m_json.is_array(); }
bool JsonObject::IsString() const { return m_json.is_string(); }
bool JsonObject::IsNumber() const { return m_json.is_number(); }
bool JsonObject::IsBoolean() const { return m_json.is_boolean(); }

bool JsonObject::ContainsKey(const std::string& key) const
{
    return m_json.is_object() && m_json.contains(key);
}

std::string JsonObject::GetString(const std::string& key, const std::string& defaultValue) const
{
    if (ContainsKey(key) && m_json[key].is_string())
    {
        return m_json[key].get<std::string>();
    }
    return defaultValue;
}

int JsonObject::GetInt(const std::string& key, int defaultValue) const
{
    if (ContainsKey(key) && m_json[key].is_number_integer())
    {
        return m_json[key].get<int>();
    }
    return defaultValue;
}

long JsonObject::GetLong(const std::string& key, long defaultValue) const
{
    if (ContainsKey(key) && m_json[key].is_number())
    {
        return m_json[key].get<long>();
    }
    return defaultValue;
}

float JsonObject::GetFloat(const std::string& key, float defaultValue) const
{
    if (ContainsKey(key) && m_json[key].is_number())
    {
        return m_json[key].get<float>();
    }
    return defaultValue;
}

double JsonObject::GetDouble(const std::string& key, double defaultValue) const
{
    if (ContainsKey(key) && m_json[key].is_number())
    {
        return m_json[key].get<double>();
    }
    return defaultValue;
}

bool JsonObject::GetBool(const std::string& key, bool defaultValue) const
{
    if (ContainsKey(key) && m_json[key].is_boolean())
    {
        return m_json[key].get<bool>();
    }
    return defaultValue;
}

JsonObject JsonObject::GetJsonObject(const std::string& key) const
{
    if (ContainsKey(key) && m_json[key].is_object())
    {
        return JsonObject(m_json[key]);
    }
    return JsonObject();
}

std::vector<JsonObject> JsonObject::GetJsonArray(const std::string& key) const
{
    std::vector<JsonObject> result;
    if (ContainsKey(key) && m_json[key].is_array())
    {
        for (const auto& item : m_json[key])
        {
            result.emplace_back(item);
        }
    }
    return result;
}

std::optional<std::string> JsonObject::GetStringOpt(const std::string& key) const
{
    if (ContainsKey(key) && m_json[key].is_string())
    {
        return m_json[key].get<std::string>();
    }
    return std::nullopt;
}

std::optional<int> JsonObject::GetIntOpt(const std::string& key) const
{
    if (ContainsKey(key) && m_json[key].is_number_integer())
    {
        return m_json[key].get<int>();
    }
    return std::nullopt;
}

std::optional<bool> JsonObject::GetBoolOpt(const std::string& key) const
{
    if (ContainsKey(key) && m_json[key].is_boolean())
    {
        return m_json[key].get<bool>();
    }
    return std::nullopt;
}

JsonObject& JsonObject::Set(const std::string& key, const std::string& value)
{
    m_json[key] = value;
    return *this;
}

JsonObject& JsonObject::Set(const std::string& key, const char* value)
{
    m_json[key] = value;
    return *this;
}

JsonObject& JsonObject::Set(const std::string& key, int value)
{
    m_json[key] = value;
    return *this;
}

JsonObject& JsonObject::Set(const std::string& key, long value)
{
    m_json[key] = value;
    return *this;
}

JsonObject& JsonObject::Set(const std::string& key, float value)
{
    m_json[key] = value;
    return *this;
}

JsonObject& JsonObject::Set(const std::string& key, double value)
{
    m_json[key] = value;
    return *this;
}

JsonObject& JsonObject::Set(const std::string& key, bool value)
{
    m_json[key] = value;
    return *this;
}

JsonObject& JsonObject::Set(const std::string& key, const JsonObject& value)
{
    m_json[key] = value.m_json;
    return *this;
}

JsonObject& JsonObject::Set(const std::string& key, const std::vector<JsonObject>& value)
{
    Json array = Json::array();
    for (const auto& obj : value)
    {
        array.push_back(obj.m_json);
    }
    m_json[key] = array;
    return *this;
}

/**
 * @brief Retrieves the size of the JSON object or array.
 *
 * This method returns the size of the contained JSON object or array.
 * If the underlying JSON value is neither an array nor an object, it returns 0.
 *
 * @return The number of elements if the JSON value is an array or the number of key-value pairs
 * if it is an object. Returns 0 if the JSON value is neither.
 */
size_t JsonObject::Size() const
{
    if (m_json.is_array())
    {
        return m_json.size();
    }
    else if (m_json.is_object())
    {
        return m_json.size();
    }
    return 0;
}

/**
 * @brief Retrieves an element from the JSON array at the specified index.
 *
 * This method attempts to access the element at the given index within the JSON value,
 * assuming it is an array. If the JSON value is not an array or the index is out of bounds,
 * it returns an empty JsonObject.
 *
 * @param index The zero-based index of the element to retrieve.
 * @return A JsonObject representing the element at the specified index if the JSON value
 * is an array and the index is valid. Otherwise, returns an empty JsonObject.
 */
JsonObject JsonObject::GetArrayElement(size_t index) const
{
    if (m_json.is_array() && index < m_json.size())
    {
        return JsonObject(m_json[index]);
    }
    return JsonObject();
}

/**
 * @brief Adds a new JSON object as an element to the current JSON array.
 *
 * If the JSON value is not already an array, it converts the value to an array
 * before adding the specified element. The provided JSON object is appended
 * to the array.
 *
 * @param element The JSON object to be added as a new array element.
 * @return A reference to the current JsonObject after adding the element.
 */
JsonObject& JsonObject::AddArrayElement(const JsonObject& element)
{
    if (!m_json.is_array())
    {
        m_json = Json::array();
    }
    m_json.push_back(element.m_json);
    return *this;
}

JsonObject& JsonObject::Remove(const std::string& key)
{
    if (m_json.is_object())
    {
        m_json.erase(key);
    }
    return *this;
}

JsonObject& JsonObject::Clear()
{
    if (m_json.is_object())
    {
        m_json.clear();
    }
    else if (m_json.is_array())
    {
        m_json = Json::array();
    }
    return *this;
}

/**
 * @brief Converts the JSON object to a string representation.
 *
 * This method generates a string representation of the JSON object, optionally formatting
 * it with indentation for improved readability.
 *
 * @param pretty If true, the output will be formatted with indentation; otherwise, it will
 * be generated as a compact single-line string.
 * @return A string representation of the JSON object.
 */
std::string JsonObject::ToString(bool pretty) const
{
    return ToJsonString(pretty ? 4 : -1);
}

std::string JsonObject::ToJsonString(int indent) const
{
    return m_json.dump(indent);
}

JsonObject JsonObject::operator[](const std::string& key) const
{
    return GetJsonObject(key);
}

JsonObject JsonObject::operator[](size_t index) const
{
    return GetArrayElement(index);
}

bool JsonObject::Has(const std::string& key) const
{
    return ContainsKey(key);
}

/// JsonArray Implementation
JsonArray::JsonArray() : m_json(Json::array())
{
}

JsonArray::JsonArray(const Json& json) : m_json(json)
{
    if (!m_json.is_array())
    {
        throw JsonException("JSON is not an array");
    }
}

size_t JsonArray::Size() const
{
    return m_json.size();
}

bool JsonArray::IsEmpty() const
{
    return m_json.empty();
}

JsonArray& JsonArray::Add(const JsonObject& obj)
{
    m_json.push_back(obj.GetJson());
    return *this;
}

JsonArray& JsonArray::Add(const std::string& value)
{
    m_json.push_back(value);
    return *this;
}

JsonArray& JsonArray::Add(int value)
{
    m_json.push_back(value);
    return *this;
}

JsonArray& JsonArray::Add(bool value)
{
    m_json.push_back(value);
    return *this;
}

JsonObject JsonArray::GetJsonObject(size_t index) const
{
    if (index < m_json.size())
    {
        return JsonObject(m_json[index]);
    }
    return JsonObject();
}

std::string JsonArray::GetString(size_t index, const std::string& defaultValue) const
{
    if (index < m_json.size() && m_json[index].is_string())
    {
        return m_json[index].get<std::string>();
    }
    return defaultValue;
}

int JsonArray::GetInt(size_t index, int defaultValue) const
{
    if (index < m_json.size() && m_json[index].is_number_integer())
    {
        return m_json[index].get<int>();
    }
    return defaultValue;
}

bool JsonArray::GetBool(size_t index, bool defaultValue) const
{
    if (index < m_json.size() && m_json[index].is_boolean())
    {
        return m_json[index].get<bool>();
    }
    return defaultValue;
}

std::string JsonArray::ToString(bool pretty) const
{
    return m_json.dump(pretty ? 4 : -1);
}

std::vector<JsonObject> JsonArray::ToJsonObjectVector() const
{
    std::vector<JsonObject> result;
    for (const auto& item : m_json)
    {
        result.emplace_back(item);
    }
    return result;
}


JsonException::JsonException(const std::string& message) : std::runtime_error("JsonException: " + message)
{
}
