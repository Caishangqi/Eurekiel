#pragma once
#include <string>

namespace resource
{
    [[nodiscard]]
    std::string GetFileExtension(const std::string& filePath);
}
