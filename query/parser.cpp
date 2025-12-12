#include "parser.h"
#include <iostream>
#include <cmath>
#include <cctype>
#include "../utils/types.h"
#include "../utils/helpers.h"
#include "../utils/sorting.h"

using namespace std;

namespace ChronoDB {

    Parser::Parser(StorageEngine& s, GraphEngine& g) : storage(s), graph(g) {}

    // ----------------------
    // UNDO
    // ----------------------
    void Parser::undo() {
        if (undoStack.empty()) {
            Helper::printError("Nothing to Undo!");
            return;
        }

        auto reverseAction = undoStack.top();
        undoStack.pop();

        // Put that reverse action into redo stack
        redoStack.push(reverseAction);

        reverseAction();
        Helper::printSuccess("Last action undone successfully.");
    }

    // ----------------------
    // REDO
    // ----------------------
    void Parser::redo() {
        if (redoStack.empty()) {
            Helper::printError("Nothing to Redo!");
            return;
        }

        auto redoAction = redoStack.top();
        redoStack.pop();

        redoAction();
        undoStack.push(redoAction);

        Helper::printSuccess("Redo executed successfully.");
    }

    // ----------------------
    // MAIN PARSE FUNCTION
    // ----------------------
    void Parser::parseAndExecute(const string& commandLine) {
        string cmdUpper = Helper::toUpper(commandLine);

        if (cmdUpper == "UNDO") { undo(); return; }
        if (cmdUpper == "REDO") { redo(); return; }
        if (cmdUpper == "EXIT") { exit(0); }

        while(!redoStack.empty()) redoStack.pop();

        Lexer lexer(commandLine);
        auto tokens = lexer.tokenize();
        if (tokens.empty()) return;

        string cmd = Helper::toUpper(tokens[0].value);

        if (cmd == "CREATE") handleCreate(tokens);
        else if (cmd == "INSERT") handleInsert(tokens);
        else if (cmd == "SELECT") handleSelect(tokens);
        else if (cmd == "UPDATE") handleUpdate(tokens);
        else if (cmd == "DELETE") handleDelete(tokens);
        else if (cmd == "GRAPH") handleGraph(tokens);
        else Helper::printError("Unknown command: " + cmd);
    }

    // ----------------------
    // CREATE TABLE
    // ----------------------
    void Parser::handleCreate(const vector<Token>& tokens) {
        if (tokens.size() < 4 || Helper::toUpper(tokens[1].value) != "TABLE") {
            Helper::printError("Syntax: CREATE TABLE <name> [TYPE] (<col> <type>, ...)");
            return;
        }

        string tableName = tokens[2].value;
        string structureType = "HEAP";

        size_t i = 3;

        // Check for optional Structure Type before '('
        // Example: CREATE TABLE Products AVL (...)
        if (i < tokens.size() && tokens[i].value != "(") {
            string type = Helper::toUpper(tokens[i].value);
            if (type == "AVL" || type == "BST" || type == "HASH" || type == "HEAP") {
                structureType = type;
                i++;
            }
        }

        if (i >= tokens.size() || tokens[i].value != "(") {
            Helper::printError("Expected '(' after table name (and optional type).");
            return;
        }
        i++; // Consume '('

        vector<Column> columns;

        while (i < tokens.size() && tokens[i].value != ")") {
            
            // Skip commas if they appear between definitions
            if (tokens[i].value == ",") {
                i++;
                continue;
            }

            if (i + 1 >= tokens.size()) {
                Helper::printError("Incomplete column definition.");
                return;
            }

            string colName = tokens[i].value;
            string colType = Helper::toUpper(tokens[i+1].value);

            if (colType != "INT" && colType != "FLOAT" && colType != "STRING") {
                Helper::printError("Invalid column type: " + colType);
                return;
            }

            columns.push_back({colName, colType});
            i += 2;
        }

        // Consume ')'
        if (i < tokens.size() && tokens[i].value == ")") {
            i++;
        }

        // Check for "USING <TYPE>" suffix if not already set (or override)
        if (i < tokens.size()) {
            if (Helper::toUpper(tokens[i].value) == "USING") {
                if (i + 1 >= tokens.size()) {
                    Helper::printError("Expected structure type after USING");
                    return;
                }
                structureType = Helper::toUpper(tokens[i+1].value);
            }
        }

        if (storage.createTable(tableName, columns, structureType)) {
            Helper::printSuccess("Table '" + tableName + "' created using " + structureType + " (" + to_string(columns.size()) + " columns)");

            undoStack.push([this, tableName]() {
                Helper::println("[UNDO] Table removed: " + tableName);
            });

        } else {
            Helper::printError("Table already exists or invalid structure.");
        }
    }

    // ----------------------
    // INSERT
    // ----------------------
    void Parser::handleInsert(const vector<Token>& tokens) {

        if (tokens.size() < 5 ||
            Helper::toUpper(tokens[1].value) != "INTO" ||
            Helper::toUpper(tokens[3].value) != "VALUES") {
            Helper::printError("Syntax: INSERT INTO <table> VALUES (<v1>, <v2> ...)");
            return;
        }

        string tableName = tokens[2].value;

        auto columns = storage.getTableColumns(tableName);
        if (columns.empty()) {
            Helper::printError("Table does not exist: " + tableName);
            return;
        }

        size_t expected = columns.size();
        
        // Collect value tokens, skipping commas and handling parentheses
        vector<string> values;
        size_t current = 4;
        bool insideParens = false;

        if (current < tokens.size() && tokens[current].value == "(") {
            insideParens = true;
            current++;
        }

        while (current < tokens.size()) {
            if (insideParens && tokens[current].value == ")") {
                current++;
                break; // End of values
            }
            
            if (tokens[current].value == ",") {
                current++;
                continue; // Skip commas
            }

            // Detect if we accidentally hit a new command or something weird (simple check)
            if (tokens[current].value == ";") break;

            values.push_back(tokens[current].value);
            current++;
        }

        if (values.size() != expected) {
            Helper::printError("Expected " + to_string(expected) + " values, got " + to_string(values.size()));
            return;
        }

        Record r;

        for (size_t i = 0; i < columns.size(); i++) {
            string value = values[i];
            string type = columns[i].type;

            try {
                if (type == "INT") {
                    r.fields.push_back(stoi(value));
                }
                else if (type == "FLOAT") {
                    r.fields.push_back(stof(value));
                }
                else {
                    r.fields.push_back(value);
                }
            } catch (...) {
                Helper::printError("Type mismatch for column " + columns[i].name);
                return;
            }
        }

        if (storage.insertRecord(tableName, r)) {
            Helper::printSuccess("Record inserted.");

            undoStack.push([this, tableName, r]() {
                int id = get<int>(r.fields[0]);
                storage.deleteRecord(tableName, id);
                Helper::printSuccess("[UNDO] Removed inserted row ID " + to_string(id));
            });

        } else {
            Helper::printError("Failed to insert.");
        }
    }

    // ----------------------
    // SELECT
    // ----------------------
    void Parser::handleSelect(const vector<Token>& tokens) {
        if (tokens.size() < 4) {
            Helper::printError("Syntax: SELECT * FROM <table> [WHERE <col> <op> <val>]");
            return;
        }

        string tableName = tokens[3].value;

        // Check for specific Algorithm Selection (BFS/DFS) (Legacy/Lab compatible)
        // Syntax: SELECT * FROM table WHERE ID 10 USING BFS
        if (tokens.size() >= 9 && 
            Helper::toUpper(tokens[4].value) == "WHERE" && 
            Helper::toUpper(tokens[5].value) == "ID" &&
            Helper::toUpper(tokens[7].value) == "USING") {
                
             int id = stoi(tokens[6].value);
             string algo = Helper::toUpper(tokens[8].value);
             
             BST* bst = storage.getBST(tableName);
             if (!bst) {
                 Helper::printError("BFS/DFS only supported on BST tables.");
                 return;
             }

             optional<Record> res;
             if (algo == "BFS") res = bst->searchBFS(id);
             else if (algo == "DFS") res = bst->searchDFS(id);
             else {
                 Helper::printError("Unknown algorithm: " + algo);
                 return;
             }

             if (res.has_value()) {
                 auto cols = storage.getTableColumns(tableName);
                 vector<string> h; for(auto& c : cols) h.push_back(c.name);
                 vector<vector<variant<int,float,string>>> r;
                 r.push_back(res->fields);
                 Helper::printTable(r, h);
             }
             return;
        }

        auto rows = storage.selectAll(tableName);
        auto columns = storage.getTableColumns(tableName);

        if (columns.empty()) {
            Helper::printError("Table does not exist.");
            return;
        }

        // Generic WHERE clause support
        // Syntax: WHERE <col> <op> <val>
        // Ops: =, <, >, <=, >=
        if (tokens.size() >= 7 && Helper::toUpper(tokens[4].value) == "WHERE") {
            string colName = tokens[5].value;
            string op = tokens[6].value;
            string valStr = tokens[7].value;

            int colIndex = -1;
            string colType = "";
            for(size_t i=0; i<columns.size(); i++) {
                if(Helper::toUpper(columns[i].name) == Helper::toUpper(colName)) {
                    colIndex = i;
                    colType = columns[i].type;
                    break;
                }
            }

            if (colIndex == -1) {
                Helper::printError("Column not found: " + colName);
                return;
            }

            // FILTER LOGIC
            // If Range Query (>, <, >=, <=) -> Use Sort + Binary Search
            // If Equality (=) -> Use Scan (or ID lookup if implemented, but keeping generic scan/filter)
            
            bool isRange = (op == ">" || op == "<" || op == ">=" || op == "<=");

            if (isRange) {
                // 1. Sort Records
                // This is O(N log N)
                Sorting::mergeSort(rows, colIndex, colType);
                
                vector<Record> filtered;
                
                // 2. Binary Search
                if (op == ">" || op == ">=") {
                     // Find first element matching
                     // For >= val: LowerBound (first element >= val)
                     // For > val: UpperBound (first element > val)
                     
                     int index = -1;
                     if (op == ">=") index = Sorting::binarySearchLowerBound(rows, colIndex, colType, valStr);
                     else index = Sorting::binarySearchUpperBound(rows, colIndex, colType, valStr); // >
                     
                     // All elements from index to end are matches
                     for(size_t i = index; i < rows.size(); i++) {
                         filtered.push_back(rows[i]);
                     }
                     
                } else if (op == "<" || op == "<=") {
                    // For < val: LowerBound (first element >= val) -> implies everything before is < val
                    // For <= val: UpperBound (first element > val) -> implies everything before is <= val
                    
                    int index = -1;
                    if (op == "<") index = Sorting::binarySearchLowerBound(rows, colIndex, colType, valStr); // index is start of >= val, so 0..index-1 are < val
                    else index = Sorting::binarySearchUpperBound(rows, colIndex, colType, valStr); // index is start of > val, so 0..index-1 are <= val
                    
                    for(size_t i = 0; i < index && i < rows.size(); i++) {
                        filtered.push_back(rows[i]);
                    }
                }
                
                rows = filtered;

            } else {
                // LINEAR SCAN for Equality (=) or others
                vector<Record> filtered;
                for(auto& r : rows) {
                    bool match = false;
                    
                    if (colType == "INT") {
                        int cell = get<int>(r.fields[colIndex]);
                        int val = stoi(valStr);
                        if (op == "=") match = (cell == val);
                    }
                    else if (colType == "FLOAT") {
                        float cell = get<float>(r.fields[colIndex]);
                        float val = stof(valStr);
                        if (op == "=") match = (abs(cell - val) < 0.0001);
                    }
                    else if (colType == "STRING") {
                        string cell = get<string>(r.fields[colIndex]);
                        if (op == "=") match = (cell == valStr);
                    }

                    if (match) filtered.push_back(r);
                }
                rows = filtered; 
            }
        }

        vector<string> headers;
        for (auto& c : columns) headers.push_back(c.name);

        if (rows.empty()) {
            Helper::printLine('-', 40);
            Helper::println("No matching rows in table " + tableName);
            return;
        }

        vector<vector<variant<int,float,string>>> tableRows;
        for (auto& rec : rows)
            tableRows.push_back(rec.fields);

        Helper::printTable(tableRows, headers);
    }



    // ----------------------
    // UPDATE
    // ----------------------
    void Parser::handleUpdate(const vector<Token>& tokens) {
        // UPDATE t SET col value WHERE ID id

        // Allow trailing semicolon (size 8 or 9)
        if (tokens.size() < 8 ||
            Helper::toUpper(tokens[2].value) != "SET" ||
            Helper::toUpper(tokens[5].value) != "WHERE" ||
            Helper::toUpper(tokens[6].value) != "ID") {
            Helper::printError("Syntax: UPDATE <table> SET <col> <value> WHERE ID <id>");
            return;
        }

        string tableName = tokens[1].value;
        string field = tokens[3].value;
        string newValue = tokens[4].value;
        int id = stoi(tokens[7].value);

        auto columns = storage.getTableColumns(tableName);
        auto rows = storage.selectAll(tableName);

        int colIndex = -1;
        for (size_t i = 0; i < columns.size(); i++) {
            if (Helper::toUpper(columns[i].name) == Helper::toUpper(field)) {
                colIndex = i;
                break;
            }
        }

        if (colIndex == -1) {
            Helper::printError("Field does not exist.");
            return;
        }

        for (auto& rec : rows) {
            if (get<int>(rec.fields[0]) == id) {
                Record old = rec;

                if (columns[colIndex].type == "INT")
                    rec.fields[colIndex] = stoi(newValue);
                else if (columns[colIndex].type == "FLOAT")
                    rec.fields[colIndex] = stof(newValue);
                else
                    rec.fields[colIndex] = newValue;

                storage.updateRecord(tableName, id, rec);

                undoStack.push([this, tableName, old]() {
                    int oldId = get<int>(old.fields[0]);
                    storage.updateRecord(tableName, oldId, old);
                    Helper::println("[UNDO] Reverted update for ID " + to_string(oldId));
                });

                Helper::printSuccess("Record updated.");
                return;
            }
        }

        Helper::printError("ID not found.");
    }

    // ----------------------
    // DELETE
    // ----------------------
    void Parser::handleDelete(const vector<Token>& tokens) {

        if (tokens.size() < 6 ||
            Helper::toUpper(tokens[1].value) != "FROM" ||
            Helper::toUpper(tokens[3].value) != "WHERE" ||
            Helper::toUpper(tokens[4].value) != "ID") {
            Helper::printError("Syntax: DELETE FROM <table> WHERE ID <id>");
            return;
        }

        string tableName = tokens[2].value;
        int id = stoi(tokens[5].value);

        auto rows = storage.selectAll(tableName);

        bool found = false;
        Record deleted;

        for (auto& rec : rows) {
            if (get<int>(rec.fields[0]) == id) {
                deleted = rec;
                found = true;
                break;
            }
        }

        if (!found) {
            Helper::printError("ID not found.");
            return;
        }

        storage.deleteRecord(tableName, id);
        Helper::printSuccess("Record deleted.");

        undoStack.push([this, tableName, deleted]() {
            storage.insertRecord(tableName, deleted);
            Helper::println("[UNDO] Restored deleted ID " + to_string(get<int>(deleted.fields[0])));
        });
    }

    // ----------------------
     // GRAPH COMMANDS
    // ----------------------
    void Parser::handleGraph(const vector<Token>& tokens) {
        if (tokens.size() < 2) {
            Helper::printError("GRAPH requires action: CREATE, ADDEDGE, IMPORT, BFS, DFS, DIJKSTRA, SHOW");
            return;
        }

        string action = Helper::toUpper(tokens[1].value);

        // Helper to strip trailing non-alphanumeric (like ;)
        auto cleanName = [](const string& in) -> string {
            string out = in;
            while (!out.empty() && !isalnum(out.back())) out.pop_back();
            return out;
        };

        // CREATE GRAPH <name>
        if (action == "CREATE" && tokens.size() >= 3) {
            graph.createGraph(cleanName(tokens[2].value));
        }
        // GRAPH IMPORT <graph> FROM <table> COLUMN <col>
        else if (action == "IMPORT") {
            // Syntax check: GRAPH IMPORT G1 FROM Cities COLUMN Name
            // Tokens: 0=GRAPH, 1=IMPORT, 2=G1, 3=FROM, 4=Cities, 5=COLUMN, 6=Name
            if (tokens.size() < 7 || 
                Helper::toUpper(tokens[3].value) != "FROM" ||
                Helper::toUpper(tokens[5].value) != "COLUMN") {
                Helper::printError("Syntax: GRAPH IMPORT <graph> FROM <table> COLUMN <col>");
                return;
            }
            
            string graphName = cleanName(tokens[2].value);
            string tableName = tokens[4].value;
            string colName = cleanName(tokens[6].value); // Clean col name too just in case

            Graph* g = graph.getGraph(graphName);
            if (!g) return; // Error printed by getGraph

           auto rows = storage.selectAll(tableName);
            auto columns = storage.getTableColumns(tableName);
            
            // Find Column Index
            int colIndex = -1;
            for(size_t i=0; i<columns.size(); i++) {
                if(Helper::toUpper(columns[i].name) == Helper::toUpper(colName)) {
                    colIndex = i;
                    break;
                }
            }

            if (colIndex == -1) {
                Helper::printError("Column not found: " + colName);
                return;
            }

            // Import
            int count = 0;
            for(auto& r : rows) {
                string valStr;
                // Convert to String for Vertex Name
                if (holds_alternative<int>(r.fields[colIndex])) 
                    valStr = to_string(get<int>(r.fields[colIndex]));
                else if (holds_alternative<float>(r.fields[colIndex]))
                    valStr = to_string(get<float>(r.fields[colIndex]));
                else
                    valStr = get<string>(r.fields[colIndex]);

                g->addVertex(valStr);
                count++;
            }
            Helper::printSuccess("Imported " + to_string(count) + " nodes into " + graphName);
        }
        // GRAPH ADDEDGE <graph> <uVal> <vVal> <weight>
        else if (action == "ADDEDGE" && tokens.size() >= 6) {
            if (auto g = graph.getGraph(cleanName(tokens[2].value)))
                g->addEdge(tokens[3].value, cleanName(tokens[4].value), stoi(tokens[5].value), false);
        }
        // GRAPH SHOW <graph>
        else if (action == "SHOW" && tokens.size() >= 3) {
            // Logic handled by GUI, but we print here for CLI
            Helper::printSuccess("Opening Visualization for " + cleanName(tokens[2].value) + "...");
        }
        else if (action == "PRINT" && tokens.size() >= 3) {
            if (auto g = graph.getGraph(cleanName(tokens[2].value)))
                g->printGraph();
        }
        else if (action == "BFS" && tokens.size() >= 4) {
            if (auto g = graph.getGraph(cleanName(tokens[2].value)))
                g->bfs(cleanName(tokens[3].value));
        }
        else if (action == "DFS" && tokens.size() >= 4) {
             if (auto g = graph.getGraph(cleanName(tokens[2].value)))
                g->dfs(cleanName(tokens[3].value));
        }
        else if (action == "DIJKSTRA" && tokens.size() >= 5) {
            if (auto g = graph.getGraph(cleanName(tokens[2].value)))
                g->dijkstra(tokens[3].value, cleanName(tokens[4].value));
        }
        else {
            Helper::printError("Unknown GRAPH command.");
        }
    }
}
