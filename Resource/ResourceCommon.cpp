#include "ResourceCommon.hpp"

#include "Engine/Core/StringUtils.hpp"

using namespace resource;

std::string GetFileExtension(const std::string& filePath)
{
    Strings split = SplitStringOnDelimiter(filePath, '.');
    if ((int)split.size() > 1)
    {
        return *split.end();
    }
    return "";
}
