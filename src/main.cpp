#include <iostream>
#include <string>
#include "../storage/storage.h"
#include "../query/parser.h"
#include "../graph/graph.h"
#include "../utils/helpers.h"

using namespace std;
using namespace ChronoDB;

int main() {
    StorageEngine storage;
    GraphEngine graph;
    Parser parser(storage, graph);

    string inputLine;
    string commandBuffer;

    cout << "=== ChronoDB SQL CLI ===" << endl;
    cout << "Type 'EXIT' to quit." << endl;

    while (true) {
        if (commandBuffer.empty())
            cout << "ChronoDB> ";
        else
            cout << "....> ";

        getline(cin, inputLine);

        if (inputLine.empty()) continue;

        // Check for EXIT (case-insensitive)
        string inputUpper = Helper::toUpper(inputLine);
        
        // Basic trim logic
        size_t last = inputLine.find_last_not_of(" \t\r\n");
        if (last == string::npos) continue; // Empty line
        
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
