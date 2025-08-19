#pragma once
#include <string>
#include <vector>

// Stub header for ConsoleCommandManager - disabled for simplified console implementation
// The simplified console forwards all commands to DevConsole instead

namespace enigma::core {
    
    // Minimal stub class to satisfy build requirements
    class ConsoleCommandManager {
    public:
        ConsoleCommandManager(class ConsoleSubsystem* console) { (void)console; }
        ~ConsoleCommandManager() = default;
        
        // Stub methods that do nothing
        const std::vector<std::string>& GetHistory() const { 
            static std::vector<std::string> empty; 
            return empty; 
        }
        
        void SaveHistoryToFile(const std::string& filePath) const { (void)filePath; }
        
        // Other methods omitted - not used in simplified console
    };

} // namespace enigma::core