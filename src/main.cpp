#include <iostream>
#include <vector>
#include <string>
#include "../storage/storage.h"
#include "../utils/types.h"
#include "../utils/helpers.h"

using namespace std;
using namespace ChronoDB;

// Helper to create a record easily
Record createStudent(int id, string name, float gpa) {
    Record rec;
    rec.fields.emplace_back(id);
    rec.fields.emplace_back(name);
    rec.fields.emplace_back(gpa);
    return rec;
}

int main() {
    Helper::printLine('=', 50);
    cout << "   CHRONODB STORAGE ENGINE TEST   " << endl;
    Helper::printLine('=', 50);

    // 1. Initialize Engine
    string dbName = "test_data";
    StorageEngine engine(dbName);
    string tableName = "students";

    // 2. Create Table
    cout << "\n[1] Creating Table '" << tableName << "'..." << endl;
    if (engine.createTable(tableName)) {
        Helper::printSuccess("Table created successfully.");
    } else {
        cout << "[INFO] Table already exists." << endl;
    }

    // 3. Insert Records
    cout << "\n[2] Inserting Records..." << endl;
    vector<Record> students = {
        createStudent(101, "Alice", 3.8f),
        createStudent(102, "Bob", 2.9f),
        createStudent(103, "Charlie", 3.5f),
        createStudent(104, "Diana", 4.0f)
    };

    for (const auto& s : students) {
        if (engine.insertRecord(tableName, s)) {
            cout << " -> Inserted: ";
            visit([](auto&& arg) { cout << arg << " "; }, s.fields[1]); // Print Name
            cout << endl;
        } else {
            Helper::printError("Failed to insert record.");
        }
    }

    // 4. Select All (Read)
    cout << "\n[3] Reading All Records..." << endl;
    vector<Record> rows = engine.selectAll(tableName);
    Helper::printLine('-', 30);
    cout << "ID  | Name       | GPA" << endl;
    Helper::printLine('-', 30);
    for (const auto& row : rows) {
        // We know the structure is INT, STRING, FLOAT
        cout << get<int>(row.fields[0]) << " | " 
             << get<string>(row.fields[1]) << "     | " 
             << get<float>(row.fields[2]) << endl;
    }

    // 5. Update Record (Bob improves his GPA)
    cout << "\n[4] Updating Bob (ID 102)..." << endl;
    Record updatedBob = createStudent(102, "Bob Updated", 3.9f);
    if (engine.updateRecord(tableName, 102, updatedBob)) {
        Helper::printSuccess("Update successful.");
    } else {
        Helper::printError("Update failed.");
    }

    // 6. Delete Record (Charlie drops out)
    cout << "\n[5] Deleting Charlie (ID 103)..." << endl;
    if (engine.deleteRecord(tableName, 103)) {
        Helper::printSuccess("Delete successful.");
    } else {
        Helper::printError("Delete failed.");
    }

    // 7. Verify Changes
    cout << "\n[6] Verifying Final State..." << endl;
    rows = engine.selectAll(tableName);
    Helper::printLine('-', 30);
    for (const auto& row : rows) {
        cout << get<int>(row.fields[0]) << " | " 
             << get<string>(row.fields[1]) << " | " 
             << get<float>(row.fields[2]) << endl;
    }

    Helper::printLine('=', 50);
    return 0;
}