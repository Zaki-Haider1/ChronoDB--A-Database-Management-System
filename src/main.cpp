#include <iostream>
#include <string>
#include "../storage/storage.h"
#include "../query/parser.h"
#include "../graph/graph.h"
#include "../utils/helpers.h"

using namespace ChronoDB;

int main() {
    StorageEngine storage;
    GraphEngine graph;
    Parser parser(storage, graph);

    std::string inputLine;
    std::string commandBuffer;

    std::cout << "=== ChronoDB SQL CLI ===" << std::endl;
    std::cout << "Type 'EXIT' to quit." << std::endl;

    while (true) {
        if (commandBuffer.empty())
            std::cout << "ChronoDB> ";
        else
            std::cout << "....> ";

        std::getline(std::cin, inputLine);

        if (inputLine.empty()) continue;

        // Check for EXIT (case-insensitive)
        // Check for EXIT (case-insensitive)
        std::string inputUpper = Helper::toUpper(inputLine);
        
        // Basic trim logic
        size_t last = inputLine.find_last_not_of(" \t\r\n");
        if (last == std::string::npos) continue; // Empty line
        
        // Remove trailing whitespace from the VIEW of the line for checking ';'
        char lastChar = inputLine[last];

        if (inputUpper.find("EXIT") == 0 || inputUpper.find("QUIT") == 0) break;

        commandBuffer += inputLine + " ";

        // If command ends with ';', execute it
        if (lastChar == ';') {
            parser.parseAndExecute(commandBuffer);
            commandBuffer.clear();
        }
    }

    return 0;
}
