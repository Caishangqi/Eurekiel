#pragma once
#include <ThirdParty/json/json.hpp>
#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace enigma::core
{
    using Json = nlohmann::json;

    /**
     * JsonObject - JSON object wrapper for handling key-value JSON structures.
     * Provides methods for creating, parsing, manipulating, and accessing JSON object data.
     */
    class JsonObject
    {
        // Constructors
        JsonObject();
        explicit JsonObject(const std::string& jsonString);
        explicit JsonObject(const Json& json);
        JsonObject(const JsonObject& other);
        JsonObject(JsonObject&& other) noexcept;

        // Assignment operators
        JsonObject& operator=(const JsonObject& other);
        JsonObject& operator=(JsonObject&& other) noexcept;

        // Static factory methods
        static JsonObject                Parse(const std::string& jsonString);
        static JsonObject                Parse(const std::vector<uint8_t>& data);
        static std::optional<JsonObject> TryParse(const std::string& jsonString);

        // Type checking
        bool IsNull() const;
        bool IsObject() const;
        bool IsArray() const;
        bool IsString() const;
        bool IsNumber() const;
        bool IsBoolean() const;

        // Key checking
        bool ContainsKey(const std::string& key) const;
        bool Has(const std::string& key) const;

        // Getters with default values
        std::string GetString(const std::string& key, const std::string& defaultValue = "") const;
        std::string GetStr(const std::string& key, const std::string& defaultValue = "") const { return GetString(key, defaultValue); }

        int    GetInt(const std::string& key, int defaultValue = 0) const;
        long   GetLong(const std::string& key, long defaultValue = 0) const;
        float  GetFloat(const std::string& key, float defaultValue = 0.0f) const;
        double GetDouble(const std::string& key, double defaultValue = 0.0) const;
        bool   GetBool(const std::string& key, bool defaultValue = false) const;

        // Get nested object
        JsonObject              GetJsonObject(const std::string& key) const;
        std::vector<JsonObject> GetJsonArray(const std::string& key) const;

        // Optional getters (return std::nullopt if key doesn't exist)
        std::optional<std::string> GetStringOpt(const std::string& key) const;
        std::optional<int>         GetIntOpt(const std::string& key) const;
        std::optional<bool>        GetBoolOpt(const std::string& key) const;

        // Setters
        JsonObject& Set(const std::string& key, const std::string& value);
        JsonObject& Set(const std::string& key, const char* value);
        JsonObject& Set(const std::string& key, int value);
        JsonObject& Set(const std::string& key, long value);
        JsonObject& Set(const std::string& key, float value);
        JsonObject& Set(const std::string& key, double value);
        JsonObject& Set(const std::string& key, bool value);
        JsonObject& Set(const std::string& key, const JsonObject& value);
        JsonObject& Set(const std::string& key, const std::vector<JsonObject>& value);

        // Array operations
        size_t      Size() const;
        JsonObject  GetArrayElement(size_t index) const;
        JsonObject& AddArrayElement(const JsonObject& element);

        // Remove operation
        JsonObject& Remove(const std::string& key);
        JsonObject& Clear();

        // Conversion
        std::string ToString(bool pretty = false) const;
        std::string ToJsonString(int indent = -1) const;

        // Get underlying nlohmann::json
        const Json& GetJson() const { return m_json; }
        Json&       GetJson() { return m_json; }

        // Operators
        JsonObject operator[](const std::string& key) const;
        JsonObject operator[](size_t index) const;

    private:
        Json m_json;
    };

    /**
     * JsonArray - JSON array wrapper for handling JSON array structures
     * Provides methods for manipulating and accessing JSON array data
     */
    class JsonArray
    {
    public:
        JsonArray();
        explicit JsonArray(const Json& json);

        // Array operations
        size_t Size() const;
        bool   IsEmpty() const;

        // Add elements
        JsonArray& Add(const JsonObject& element);
        JsonArray& Add(const std::string& value);
        JsonArray& Add(int value);
        JsonArray& Add(bool value);

        // Get elements
        JsonObject  GetJsonObject(size_t index) const;
        std::string GetString(size_t index, const std::string& defaultValue = "") const;
        int         GetInt(size_t index, int defaultValue = 0) const;
        bool        GetBool(size_t index, bool defaultValue = false) const;

        // Conversion
        std::string             ToString(bool pretty = false) const;
        std::vector<JsonObject> ToJsonObjectVector() const;

        // Get underlying json
        const Json& GetJson() const { return m_json; }

    private:
        Json m_json;
    };

    class JsonException : public std::runtime_error
    {
    public:
        explicit JsonException(const std::string& message);
    };
}
