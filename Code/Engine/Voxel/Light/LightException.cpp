#include "LightException.hpp"
using namespace enigma::voxel;

LightEngineException::LightEngineException(const std::string& message) : m_message(message)
{
}

const char* LightEngineException::what() const noexcept
{
    return m_message.c_str();
}
