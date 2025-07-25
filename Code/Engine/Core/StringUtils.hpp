#pragma once
//-----------------------------------------------------------------------------------------------
#include <string>
#include <vector>

using Strings = std::vector<std::string>;

//-----------------------------------------------------------------------------------------------
const std::string Stringf(const char* format, ...);
const std::string Stringf(int maxLength, const char* format, ...);
Strings           SplitStringOnDelimiter(const std::string& originalString, char delimiterToSplitOn);
