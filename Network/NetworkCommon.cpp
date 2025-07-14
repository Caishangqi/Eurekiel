#include "NetworkCommon.hpp"

/**
 * Converts a given string into a vector of bytes, where each character in the
 * string is cast to its corresponding uint8_t representation.
 *
 * @param str A reference to the string that will be converted to a vector of bytes.
 * @return A vector of uint8_t containing the byte representation of the input string.
 */
std::vector<uint8_t> StringToByte(std::string& str)
{
    std::vector<uint8_t> result;
    result.reserve(str.size());
    for (char c : str)
    {
        result.push_back(static_cast<uint8_t>(c));
    }
    return result;
}
