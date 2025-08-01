#include "Json.hpp"

using namespace enigma::core;

JsonException::JsonException(const std::string& message) : std::runtime_error("JsonException: " + message)
{
}
