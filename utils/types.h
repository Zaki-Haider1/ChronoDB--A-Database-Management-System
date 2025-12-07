#ifndef UTILS_TYPES_H
#define UTILS_TYPES_H

#include <string>
#include <vector>
#include <variant>
#include <iostream>
using namespace std;

enum class DataType {
    INT,
    FLOAT,
    STRING
};

inline string DataTypeToString(DataType type) {
    switch (type) {
        case DataType::INT: return "INT";
        case DataType::FLOAT: return "FLOAT";
        case DataType::STRING: return "STRING";
        default: return "UNKNOWN";
    }
}

using RecordValue = variant<int, float, string>;

inline void printRecordValue(const RecordValue& value) {
    visit([](auto&& arg) { cout << arg; }, value);
}

// Add Record struct
struct Record {
    vector<RecordValue> fields;
};

// Table schema
struct TableSchema {
    string tableName;
    vector<string> columnNames;
    vector<DataType> columnTypes;

    int primaryKeyIndex = -1;
    int columnCount() const {
        return columnNames.size();
    }
};

#endif
