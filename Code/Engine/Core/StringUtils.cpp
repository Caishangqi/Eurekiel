#include "Engine/Core/StringUtils.hpp"
#include <stdarg.h>

#include "ErrorWarningAssert.hpp"


//-----------------------------------------------------------------------------------------------
constexpr int STRINGF_STACK_LOCAL_TEMP_LENGTH = 2048;


//-----------------------------------------------------------------------------------------------
const std::string Stringf(const char* format, ...)
{
    char    textLiteral[STRINGF_STACK_LOCAL_TEMP_LENGTH];
    va_list variableArgumentList;
    va_start(variableArgumentList, format);
    vsnprintf_s(textLiteral, STRINGF_STACK_LOCAL_TEMP_LENGTH, _TRUNCATE, format, variableArgumentList);
    va_end(variableArgumentList);
    textLiteral[STRINGF_STACK_LOCAL_TEMP_LENGTH - 1] = '\0'; // In case vsnprintf overran (doesn't auto-terminate)

    return std::string(textLiteral);
}


//-----------------------------------------------------------------------------------------------
const std::string Stringf(int maxLength, const char* format, ...)
{
    char  textLiteralSmall[STRINGF_STACK_LOCAL_TEMP_LENGTH];
    char* textLiteral = textLiteralSmall;
    if (maxLength > STRINGF_STACK_LOCAL_TEMP_LENGTH)
        textLiteral = new char[maxLength];

    va_list variableArgumentList;
    va_start(variableArgumentList, format);
    vsnprintf_s(textLiteral, maxLength, _TRUNCATE, format, variableArgumentList);
    va_end(variableArgumentList);
    textLiteral[maxLength - 1] = '\0'; // In case vsnprintf overran (doesn't auto-terminate)

    std::string returnValue(textLiteral);
    if (maxLength > STRINGF_STACK_LOCAL_TEMP_LENGTH)
        delete[] textLiteral;

    return returnValue;
}

Strings SplitStringOnDelimiter(const std::string& originalString, char delimiterToSplitOn)
{
    Strings                result;
    std::string::size_type start = 0;
    std::string::size_type end   = originalString.find(delimiterToSplitOn);

    while (end != std::string::npos)
    {
        result.push_back(originalString.substr(start, end - start));
        start = end + 1;
        end   = originalString.find(delimiterToSplitOn, start);
    }

    result.push_back(originalString.substr(start)); // The last one push in the stirng list

    return result;
}
