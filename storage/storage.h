#ifndef CHRONODB_STORAGE_H
#define CHRONODB_STORAGE_H

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include <optional>
#include "../utils/types.h"
using namespace std;

namespace ChronoDB {

    // Page constants
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
    };

    class StorageEngine {
    public:
        StorageEngine(const string& storageDir = "./data");
        ~StorageEngine();
        
        bool createTable(const string& tableName);
        bool insertRecord(const string& tableName, const Record& rec);
        vector<Record> selectAll(const string& tableName);

        bool updateRecord(const string& tableName, int id, const Record& newRecord);
        bool deleteRecord(const string& tableName, int id);

        bool writePageToFile(const string& tableName, uint32_t pageIndex, const Page& page);
        bool readPageFromFile(const string& tableName, uint32_t pageIndex, Page& outPage);

    private:
        string storageDirectory;

        string tableDataPath(const string& tableName) const;
        string tableMetaPath(const string& tableName) const;

        static void serializeRecord(const Record& r, vector<uint8_t>& out);
        static bool deserializeRecord(const vector<uint8_t>& in, Record& out);

        uint32_t pageCount(const string& tableName) const;
        uint32_t appendEmptyPage(const string& tableName);
    };

} // namespace ChronoDB

#endif // CHRONODB_STORAGE_H
