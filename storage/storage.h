#ifndef CHRONODB_STORAGE_H
#define CHRONODB_STORAGE_H

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <optional>
#include "../utils/types.h"
#include "../utils/types.h"
#include <unordered_map>
#include "../src/structures/avl_tree.h"
#include "../src/structures/bst.h"
#include "../src/structures/hash_table.h"
using namespace std;

namespace ChronoDB {

    // Column description
    struct Column {
        string name;   // column name (e.g., id, name)
        string type;   // INT, FLOAT, STRING
    };

    // Full schema for a table
    struct TableSchema {
        vector<Column> columns;
        string primaryKey;
    }; // <-- FIXED missing semicolon

    // -------- Page Constants --------
    static constexpr uint32_t PAGE_SIZE = 8192; // 8 KB
    static constexpr uint16_t PAGE_HEADER_RESERVED = 64;

    struct SlotEntry {
        uint16_t offset;
        uint16_t length;
        bool active;
        SlotEntry(uint16_t o = 0, uint16_t l = 0, bool a = true)
            : offset(o), length(l), active(a) {}
    };

    struct Page {
        uint32_t pageID = 0;
        uint16_t slotCount = 0;
        uint16_t freeSpaceOffset = PAGE_HEADER_RESERVED;

        vector<SlotEntry> slots;
        vector<uint8_t> data;

        Page() {
            data.resize(PAGE_SIZE, 0);
        }

        uint16_t usedDataBytes() const { return freeSpaceOffset; }
        uint16_t freeSpace() const;
        optional<uint16_t> insertRawRecord(const vector<uint8_t>& rec);
        bool deleteSlot(uint16_t slotID);
        bool readRawRecord(uint16_t slotID, vector<uint8_t>& out) const;

        void serializeToBuffer(vector<uint8_t>& buffer) const;
        void deserializeFromBuffer(const vector<uint8_t>& buffer);
    };

    struct TableMeta {
        string tableName;
        vector<Column> columns;
    };

    class StorageEngine {
    public:
        StorageEngine(const string& storageDir = "./data");
        ~StorageEngine();

        // Create table WITH schema
        bool createTable(const string& tableName, const vector<Column>& columns);

        // Old version (no schema)
        bool createTable(const string& tableName);

        bool insertRecord(const string& tableName, const Record& rec);
        vector<Record> selectAll(const string& tableName);

        bool updateRecord(const string& tableName, int id, const Record& newRecord);
        bool deleteRecord(const string& tableName, int id);

        bool writePageToFile(const string& tableName, uint32_t pageIndex, const Page& page);
        bool readPageFromFile(const string& tableName, uint32_t pageIndex, Page& outPage);

        // Schema access
        vector<Column> getTableColumns(const string& tableName) const;

        // === NEW: Full schema I/O ===
        TableSchema loadSchema(const string& tableName) const;
        bool saveSchema(const string& tableName, const TableSchema& schema) const;

    private:
        string storageDirectory;

        string tableDataPath(const string& tableName) const;
        string tableMetaPath(const string& tableName) const;

        static void serializeRecord(const Record& r, vector<uint8_t>& out);
        static bool deserializeRecord(const vector<uint8_t>& in, Record& out);
        vector<Record> loadAllRecords(const string& tableName) const;

        uint32_t pageCount(const string& tableName) const;
        uint32_t appendEmptyPage(const string& tableName);

        // helpers
        bool writeMetaFile(const string& tableName, const vector<Column>& columns) const;
        optional<vector<Column>> readMetaFile(const string& tableName) const;
        static bool typeStringMatchesValue(const string& typeStr, const RecordValue& v);

        // --- Multi-Structure Management ---
        enum class StructureType { HEAP, AVL, BST, HASH };
        
        // Registry: TableName -> StructureType
        unordered_map<string, StructureType> tableStructures;

        // In-Memory Structures (since we aren't persisting them to disk for this project demo)
        // TableName -> Instance
        unordered_map<string, AVLTree> avlTables;
        unordered_map<string, BST> bstTables;
        unordered_map<string, HashTable> hashTables;

    public:
        // Expose method to create with specific structure
        bool createTable(const string& tableName, const vector<Column>& columns, const string& structureType);
        
        // Expose method to get structure type
        StructureType getStructureType(const string& tableName) const;
        
        // Expose getters for specific tables (for Parser access to BFS/DFS)
        BST* getBST(const string& tableName) {
             if (bstTables.find(tableName) != bstTables.end()) return &bstTables[tableName];
             return nullptr;
        }

    // Closing brace from original class
    };

} // namespace ChronoDB

#endif // CHRONODB_STORAGE_H
