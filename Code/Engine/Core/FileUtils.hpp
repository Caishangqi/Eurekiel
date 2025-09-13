#pragma once
#include <string>
#include <vector>

int FileReadToBuffer(std::vector<uint8_t>& outBuffer, const std::string& filename);
int FileReadToString(std::string& outString, const std::string& filename);

// Plan:
// 1. Add Does File exist.
// 2. Add Write Buffer to FILENAME_MAX..
