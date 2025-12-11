#include "parser.h"
#include <iostream>
#include "../utils/types.h"
#include "../utils/helpers.h"

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
    void Parser::parseAndExecute(const std::string& commandLine) {
        std::string cmdUpper = Helper::toUpper(commandLine);

        if (cmdUpper == "UNDO") { undo(); return; }
        if (cmdUpper == "REDO") { redo(); return; }

        while(!redoStack.empty()) redoStack.pop();

        Lexer lexer(commandLine);
        auto tokens = lexer.tokenize();
        if (tokens.empty()) return;

        std::string cmd = Helper::toUpper(tokens[0].value);

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
    void Parser::handleCreate(const std::vector<Token>& tokens) {
        if (tokens.size() < 4 || Helper::toUpper(tokens[1].value) != "TABLE") {
            Helper::printError("Syntax: CREATE TABLE <name> (<col> <type>, ...)");
            return;
        }

        std::string tableName = tokens[2].value;
        std::string structureType = "HEAP";

        size_t i = 3;

        if (tokens[i].value != "(") {
            Helper::printError("Expected '(' after table name.");
            return;
        }
        i++;

        std::vector<Column> columns;

        while (i < tokens.size() && tokens[i].value != ")") {

            if (i + 1 >= tokens.size()) {
                Helper::printError("Incomplete column definition.");
                return;
            }

            std::string colName = tokens[i].value;
            std::string colType = Helper::toUpper(tokens[i+1].value);

            if (colType != "INT" && colType != "FLOAT" && colType != "STRING") {
                Helper::printError("Invalid column type: " + colType);
                return;
            }

            columns.push_back({colName, colType});
            i += 2;

            if (i < tokens.size() && tokens[i].value == ",")
                i++;
        }

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
            Helper::printSuccess("Table '" + tableName + "' created using " + structureType + " (" + std::to_string(columns.size()) + " columns)");

            undoStack.push([this, tableName]() {
                std::cout << "[UNDO] Table removed: " << tableName << std::endl;
            });

        } else {
            Helper::printError("Table already exists or invalid structure.");
        }
    }

    // ----------------------
    // INSERT
    // ----------------------
    void Parser::handleInsert(const std::vector<Token>& tokens) {

        if (tokens.size() < 5 ||
            Helper::toUpper(tokens[1].value) != "INTO" ||
            Helper::toUpper(tokens[3].value) != "VALUES") {
            Helper::printError("Syntax: INSERT INTO <table> VALUES <v1> <v2> ...");
            return;
        }

        std::string tableName = tokens[2].value;

        auto columns = storage.getTableColumns(tableName);
        if (columns.empty()) {
            Helper::printError("Table does not exist: " + tableName);
            return;
        }

        size_t expected = columns.size();
        size_t provided = tokens.size() - 4;

        if (expected != provided) {
            Helper::printError("Expected " + std::to_string(expected) + " values, got " + std::to_string(provided));
            return;
        }

        Record r;

        for (size_t i = 0; i < columns.size(); i++) {
            std::string value = tokens[4 + i].value;
            std::string type = columns[i].type;

            if (type == "INT") {
                r.fields.push_back(std::stoi(value));
            }
            else if (type == "FLOAT") {
                r.fields.push_back(std::stof(value));
            }
            else {
                r.fields.push_back(value);
            }
        }

        if (storage.insertRecord(tableName, r)) {
            Helper::printSuccess("Record inserted.");

            undoStack.push([this, tableName, r]() {
                int id = std::get<int>(r.fields[0]);
                storage.deleteRecord(tableName, id);
                std::cout << "[UNDO] Removed inserted row ID " << id << std::endl;
            });

        } else {
            Helper::printError("Failed to insert.");
        }
    }

    // ----------------------
    // SELECT
    // ----------------------
    void Parser::handleSelect(const std::vector<Token>& tokens) {
        if (tokens.size() < 4) {
            Helper::printError("Syntax: SELECT * FROM <table> [WHERE ID <id> USING BFS|DFS]");
            return;
        }

        std::string tableName = tokens[3].value;

        // Check for specific Algorithm Selection (BFS/DFS)
        // Syntax: SELECT * FROM table WHERE ID 10 USING BFS
        bool usingWithId = false;
        if (tokens.size() >= 9) {
             if (Helper::toUpper(tokens[4].value) == "WHERE" && 
                 Helper::toUpper(tokens[5].value) == "ID") {
                     if (Helper::toUpper(tokens[7].value) == "USING") {
                         usingWithId = true;
                         int id = std::stoi(tokens[6].value);
                         std::string algo = Helper::toUpper(tokens[8].value);
                         
                         BST* bst = storage.getBST(tableName);
                         if (!bst) {
                             Helper::printError("BFS/DFS only supported on BST tables.");
                             return;
                         }

                         std::optional<Record> res;
                         if (algo == "BFS") res = bst->searchBFS(id);
                         else if (algo == "DFS") res = bst->searchDFS(id);
                         else {
                             Helper::printError("Unknown algorithm: " + algo);
                             return;
                         }

                         if (res.has_value()) {
                             auto cols = storage.getTableColumns(tableName);
                             std::vector<std::string> h; for(auto& c : cols) h.push_back(c.name);
                             std::vector<std::vector<std::variant<int,float,std::string>>> r;
                             r.push_back(res->fields);
                             Helper::printTable(r, h);
                         }
                         return;
                     }
                 }
        }

        auto rows = storage.selectAll(tableName);
        auto columns = storage.getTableColumns(tableName);

        if (columns.empty()) {
            Helper::printError("Table does not exist.");
            return;
        }

        std::vector<std::string> headers;
        for (auto& c : columns) headers.push_back(c.name);

        if (rows.empty()) {
            Helper::printLine('-', 40);
            std::cout << "No rows in table " << tableName << std::endl;
            return;
        }

        std::vector<std::vector<std::variant<int,float,std::string>>> tableRows;

        for (auto& rec : rows)
            tableRows.push_back(rec.fields);

        Helper::printTable(tableRows, headers);
    }

    // ----------------------
    // UPDATE
    // ----------------------
    void Parser::handleUpdate(const std::vector<Token>& tokens) {
        // UPDATE t SET col value WHERE ID id

        if (tokens.size() != 8 ||
            Helper::toUpper(tokens[2].value) != "SET" ||
            Helper::toUpper(tokens[5].value) != "WHERE" ||
            Helper::toUpper(tokens[6].value) != "ID") {
            Helper::printError("Syntax: UPDATE <table> SET <col> <value> WHERE ID <id>");
            return;
        }

        std::string tableName = tokens[1].value;
        std::string field = tokens[3].value;
        std::string newValue = tokens[4].value;
        int id = std::stoi(tokens[7].value);

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
            if (std::get<int>(rec.fields[0]) == id) {
                Record old = rec;

                if (columns[colIndex].type == "INT")
                    rec.fields[colIndex] = std::stoi(newValue);
                else if (columns[colIndex].type == "FLOAT")
                    rec.fields[colIndex] = std::stof(newValue);
                else
                    rec.fields[colIndex] = newValue;

                storage.updateRecord(tableName, id, rec);

                undoStack.push([this, tableName, old]() {
                    int oldId = std::get<int>(old.fields[0]);
                    storage.updateRecord(tableName, oldId, old);
                    std::cout << "[UNDO] Reverted update for ID " << oldId << std::endl;
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
    void Parser::handleDelete(const std::vector<Token>& tokens) {

        if (tokens.size() != 6 ||
            Helper::toUpper(tokens[1].value) != "FROM" ||
            Helper::toUpper(tokens[3].value) != "WHERE" ||
            Helper::toUpper(tokens[4].value) != "ID") {
            Helper::printError("Syntax: DELETE FROM <table> WHERE ID <id>");
            return;
        }

        std::string tableName = tokens[2].value;
        int id = std::stoi(tokens[5].value);

        auto rows = storage.selectAll(tableName);

        bool found = false;
        Record deleted;

        for (auto& rec : rows) {
            if (std::get<int>(rec.fields[0]) == id) {
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
            std::cout << "[UNDO] Restored deleted ID " << std::get<int>(deleted.fields[0]) << std::endl;
        });
    }

    // ----------------------
    // GRAPH COMMANDS
    // ----------------------
    void Parser::handleGraph(const std::vector<Token>& tokens) {
        if (tokens.size() < 2) {
            Helper::printError("GRAPH requires action.");
            return;
        }

        std::string action = Helper::toUpper(tokens[1].value);

        if (action == "CREATE" && tokens.size() == 3) {
            graph.createGraph(tokens[2].value);
        }
        else if (action == "DELETE" && tokens.size() == 3) {
            graph.deleteGraph(tokens[2].value);
        }
        else if (action == "ADDVERTEX" && tokens.size() == 4) {
            if (auto g = graph.getGraph(tokens[2].value))
                g->addVertex(tokens[3].value);
        }
        else if (action == "REMOVEVERTEX" && tokens.size() == 4) {
            if (auto g = graph.getGraph(tokens[2].value))
                g->removeVertex(tokens[3].value);
        }
        else if (action == "ADDEDGE" && tokens.size() == 6) {
            if (auto g = graph.getGraph(tokens[2].value))
                g->addEdge(tokens[3].value, tokens[4].value, std::stoi(tokens[5].value), false);
        }
        else if (action == "PRINT" && tokens.size() == 3) {
            if (auto g = graph.getGraph(tokens[2].value))
                g->printGraph();
        }
        else if (action == "BFS" && tokens.size() == 4) {
            if (auto g = graph.getGraph(tokens[2].value))
                g->bfs(tokens[3].value);
        }
        else if (action == "DFS" && tokens.size() == 4) {
            if (auto g = graph.getGraph(tokens[2].value))
                g->dfs(tokens[3].value);
        }
        else if (action == "DIJKSTRA" && tokens.size() == 5) {
            if (auto g = graph.getGraph(tokens[2].value))
                g->dijkstra(tokens[3].value, tokens[4].value);
        }
        else {
            Helper::printError("Invalid GRAPH command.");
        }
    }

}
