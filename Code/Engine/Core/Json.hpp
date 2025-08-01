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
     * JsonObject - JSON object wrapper similar to Java's JSONObject
     * Provides a convenient API for JSON manipulation
     */
    class JsonObject
    {
    };

    /**
     * JsonArray - JSON array wrapper
     * Provides a convenient API for manipulating JSON arrays
     */
    class JsonArray
    {
    public:
    };

    class JsonException : public std::runtime_error
    {
    public:
        explicit JsonException(const std::string& message);
    };
}
