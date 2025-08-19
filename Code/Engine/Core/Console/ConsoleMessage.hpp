#pragma once
#include <string>
#include <chrono>
#include "Engine/Core/Logger/LogLevel.hpp"
#include "Engine/Core/Rgba8.hpp"

namespace enigma::core {

    struct ConsoleMessage {
        std::string text;
        LogLevel level = LogLevel::INFO;
        Rgba8 color = Rgba8::WHITE;
        std::chrono::system_clock::time_point timestamp;
        
        ConsoleMessage() : timestamp(std::chrono::system_clock::now()) {}
        
        ConsoleMessage(const std::string& messageText, 
                      LogLevel messageLevel = LogLevel::INFO, 
                      const Rgba8& messageColor = Rgba8::WHITE)
            : text(messageText)
            , level(messageLevel) 
            , color(messageColor)
            , timestamp(std::chrono::system_clock::now()) {}
    };

} // namespace enigma::core