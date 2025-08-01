#include "Json.hpp"

using namespace enigma::core;

bool JsonObject::Has(const std::string& key) const
{
    return ContainsKey(key);
}

JsonException::JsonException(const std::string& message) : std::runtime_error("JsonException: " + message)
{
}
