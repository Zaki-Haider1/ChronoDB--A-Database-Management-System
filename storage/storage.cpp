// storage.cpp
#include "storage.h"
#include <filesystem>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <sstream>
using namespace std;
namespace fs = std::filesystem;

namespace ChronoDB {

    // ---------- Page ----------
    uint16_t Page::freeSpace() const {
        uint32_t slotDirBytes = static_cast<uint32_t>(slots.size()) * sizeof(SlotEntry);
        if (usedDataBytes() + slotDirBytes >= PAGE_SIZE) return 0;
        return static_cast<uint16_t>(PAGE_SIZE - usedDataBytes() - slotDirBytes);
    }

    optional<uint16_t> Page::insertRawRecord(const vector<uint8_t>& rec) {
        uint16_t need = static_cast<uint16_t>(rec.size());
        uint16_t slotOverhead = sizeof(SlotEntry);
        if (freeSpace() < need + slotOverhead) return nullopt;

        memcpy(data.data() + freeSpaceOffset, rec.data(), need);
        slots.emplace_back(freeSpaceOffset, need, true);
        uint16_t slotID = static_cast<uint16_t>(slots.size() - 1);
        freeSpaceOffset += need;
        slotCount = static_cast<uint16_t>(slots.size());
        return slotID;
    }

    bool Page::deleteSlot(uint16_t slotID) {
        if (slotID >= slots.size() || !slots[slotID].active) return false;
        slots[slotID].active = false;
        return true;
    }

    bool Page::readRawRecord(uint16_t slotID, vector<uint8_t>& out) const {
        if (slotID >= slots.size() || !slots[slotID].active) return false;
        const SlotEntry& s = slots[slotID];
        if (s.offset + s.length > PAGE_SIZE) return false;
        out.resize(s.length);
        memcpy(out.data(), data.data() + s.offset, s.length);
        return true;
    }

    void Page::serializeToBuffer(vector<uint8_t>& buffer) const {
        buffer.assign(PAGE_SIZE, 0);
        memcpy(buffer.data(), &pageID, sizeof(pageID));
        memcpy(buffer.data() + 8, &slotCount, sizeof(slotCount));
        memcpy(buffer.data() + 10, &freeSpaceOffset, sizeof(freeSpaceOffset));
        if (freeSpaceOffset > PAGE_HEADER_RESERVED)
            memcpy(buffer.data() + PAGE_HEADER_RESERVED, data.data() + PAGE_HEADER_RESERVED, freeSpaceOffset - PAGE_HEADER_RESERVED);

        size_t pos = PAGE_SIZE;
        for (int i = static_cast<int>(slots.size()) - 1; i >= 0; --i) {
            const SlotEntry& s = slots[i];
            pos -= 5;
            buffer[pos] = s.active ? 1 : 0;
            memcpy(buffer.data() + pos + 1, &s.length, 2);
            memcpy(buffer.data() + pos + 3, &s.offset, 2);
        }
    }

    void Page::deserializeFromBuffer(const vector<uint8_t>& buffer) {
        if (buffer.size() < PAGE_SIZE) return;
        memcpy(&pageID, buffer.data(), sizeof(pageID));
        memcpy(&slotCount, buffer.data() + 8, sizeof(slotCount));
        memcpy(&freeSpaceOffset, buffer.data() + 10, sizeof(freeSpaceOffset));

        if (freeSpaceOffset > PAGE_HEADER_RESERVED)
            memcpy(data.data() + PAGE_HEADER_RESERVED, buffer.data() + PAGE_HEADER_RESERVED, freeSpaceOffset - PAGE_HEADER_RESERVED);

        slots.clear();
        size_t pos = PAGE_SIZE;
        for (uint16_t i = 0; i < slotCount; ++i) {
            if (pos < 5) break;
            pos -= 5;
            bool active = buffer[pos] != 0;
            uint16_t len = 0, off = 0;
            memcpy(&len, buffer.data() + pos + 1, 2);
            memcpy(&off, buffer.data() + pos + 3, 2);
            slots.emplace_back(off, len, active);
        }
        reverse(slots.begin(), slots.end());
    }

    // ---------- StorageEngine ----------
    StorageEngine::StorageEngine(const string& storageDir) : storageDirectory(storageDir) {
        if (!fs::exists(storageDirectory))
            fs::create_directories(storageDirectory);
        // (In a real system we would load the 'tableStructures' registry from disk here)
    }

    StorageEngine::~StorageEngine() {}

    string StorageEngine::tableDataPath(const string& tableName) const {
        return storageDirectory + "/" + tableName + ".tbl";
    }

    string StorageEngine::tableMetaPath(const string& tableName) const {
        return storageDirectory + "/" + tableName + ".meta";
    }

    // New createTable with columns (writes meta + empty tbl)

    // Backwards-compatible createTable that writes an empty table with no meta
    bool StorageEngine::createTable(const string& tableName) {
       return createTable(tableName, {}, "HEAP");
    }

    bool StorageEngine::createTable(const string& tableName, const vector<Column>& columns) {
        return createTable(tableName, columns, "HEAP");
    }

    bool StorageEngine::createTable(const string& tableName, const vector<Column>& columns, const string& structureType) {
        // 1. Check if already exists in memory registry
        if (tableStructures.find(tableName) != tableStructures.end()) return false;

        // 2. Register type
        if (structureType == "AVL") {
            tableStructures[tableName] = StructureType::AVL;
            avlTables[tableName] = AVLTree(); 
        } else if (structureType == "BST") {
            tableStructures[tableName] = StructureType::BST;
            bstTables[tableName] = BST();
        } else if (structureType == "HASH") {
            tableStructures[tableName] = StructureType::HASH;
            hashTables[tableName] = HashTable();
        } else {
            tableStructures[tableName] = StructureType::HEAP;
        }

        // 3. Persist metadata (schema) to disk regardless of structure
        // This allows us to know columns even if data is in memory
        if (readMetaFile(tableName).has_value()) return false; 
        
        // Write meta file (if columns provided)
        if (!columns.empty()) {
             if (!writeMetaFile(tableName, columns)) return false;
        } else {
             // For legacy empty create
             vector<Column> cols;
             writeMetaFile(tableName, cols);
        }

        // 4. If HEAP, create the empty page file
        if (tableStructures[tableName] == StructureType::HEAP) {
             string path = tableDataPath(tableName);
             ofstream file(path, ios::binary);
             if (!file) return false;
             // Write standard empty page
             Page p;
             p.pageID = 0;
             vector<uint8_t> buffer;
             p.serializeToBuffer(buffer);
             file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
             file.close();
        }

        return true;
    }

    uint32_t StorageEngine::pageCount(const string& tableName) const {
        string path = tableDataPath(tableName);
        if (!fs::exists(path)) return 0;
        ifstream in(path, ios::binary | ios::ate);
        streamsize size = in.tellg();
        in.close();
        return static_cast<uint32_t>((size + PAGE_SIZE - 1) / PAGE_SIZE);
    }

    uint32_t StorageEngine::appendEmptyPage(const string& tableName) {
        string path = tableDataPath(tableName);
        ofstream out(path, ios::binary | ios::app);
        Page p;
        p.pageID = pageCount(tableName);
        vector<uint8_t> buffer;
        p.serializeToBuffer(buffer);
        out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        out.close();
        return p.pageID;
    }

    bool StorageEngine::writePageToFile(const string& tableName, uint32_t pageIndex, const Page& page) {
        fstream fsout(tableDataPath(tableName), ios::binary | ios::in | ios::out);
        if (!fsout) { fsout.open(tableDataPath(tableName), ios::binary | ios::out); fsout.close(); fsout.open(tableDataPath(tableName), ios::binary | ios::in | ios::out); }
        vector<uint8_t> buffer;
        page.serializeToBuffer(buffer);
        fsout.seekp(static_cast<streampos>(pageIndex) * PAGE_SIZE);
        fsout.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        fsout.flush(); fsout.close();
        return true;
    }

    bool StorageEngine::readPageFromFile(const string& tableName, uint32_t pageIndex, Page& outPage) {
        ifstream infile(tableDataPath(tableName), ios::binary);
        infile.seekg(static_cast<streampos>(pageIndex) * PAGE_SIZE);
        vector<uint8_t> buffer(PAGE_SIZE);
        infile.read(reinterpret_cast<char*>(buffer.data()), PAGE_SIZE);
        infile.close();
        outPage.deserializeFromBuffer(buffer);
        return true;
    }

    // record serialisation (unchanged)
    void StorageEngine::serializeRecord(const Record& r, vector<uint8_t>& out) {
        out.clear();
        uint16_t fieldCount = static_cast<uint16_t>(r.fields.size());
        out.resize(2); memcpy(out.data(), &fieldCount, 2);

        for (const RecordValue& v : r.fields) {
            uint8_t typeTag = 0;
            if (holds_alternative<int>(v)) typeTag = 0;
            else if (holds_alternative<float>(v)) typeTag = 1;
            else typeTag = 2;

            size_t prev = out.size(); out.resize(prev + 1); out[prev] = typeTag;

            if (typeTag == 0) {
                int32_t x = get<int>(v);
                size_t cur = out.size(); out.resize(cur + sizeof(int32_t));
                memcpy(out.data() + cur, &x, sizeof(int32_t));
            } else if (typeTag == 1) {
                float f = get<float>(v);
                size_t cur = out.size(); out.resize(cur + sizeof(float));
                memcpy(out.data() + cur, &f, sizeof(float));
            } else {
                const string& s = get<string>(v);
                uint16_t len = static_cast<uint16_t>(s.size());
                size_t cur = out.size(); out.resize(cur + 2 + len);
                memcpy(out.data() + cur, &len, 2);
                memcpy(out.data() + cur + 2, s.data(), len);
            }
        }
    }

    bool StorageEngine::deserializeRecord(const vector<uint8_t>& in, Record& out) {
        out.fields.clear();
        if (in.size() < 2) return false;
        uint16_t fieldCount = 0; memcpy(&fieldCount, in.data(), 2);
        size_t pos = 2;

        for (uint16_t i = 0; i < fieldCount; ++i) {
            if (pos >= in.size()) return false;
            uint8_t typeTag = in[pos]; pos += 1;
            if (typeTag == 0) {
                if (pos + 4 > in.size()) return false;
                int32_t x; memcpy(&x, in.data() + pos, 4); pos += 4;
                out.fields.emplace_back(x);
            } else if (typeTag == 1) {
                if (pos + 4 > in.size()) return false;
                float f; memcpy(&f, in.data() + pos, 4); pos += 4;
                out.fields.emplace_back(f);
            } else {
                if (pos + 2 > in.size()) return false;
                uint16_t len = 0; memcpy(&len, in.data() + pos, 2); pos += 2;
                if (pos + len > in.size()) return false;
                string s(reinterpret_cast<const char*>(in.data() + pos), len);
                pos += len;
                out.fields.emplace_back(s);
            }
        }
        return true;
    }

    // helper: check a schema type string matches a RecordValue
    bool StorageEngine::typeStringMatchesValue(const string& typeStr, const RecordValue& v) {
        string t = typeStr;
        transform(t.begin(), t.end(), t.begin(), ::toupper);
        if (t == "INT") return holds_alternative<int>(v);
        if (t == "FLOAT") return holds_alternative<float>(v);
        if (t == "STRING") return holds_alternative<string>(v);
        return false;
    }

    bool StorageEngine::insertRecord(const string& tableName, const Record& rec) {
        if (tableStructures.find(tableName) == tableStructures.end()) {
             // Try to load from disk if not in memory (legacy support)
             if (readMetaFile(tableName).has_value()) {
                 tableStructures[tableName] = StructureType::HEAP;
             } else {
                 return false;
             }
        }

        switch (tableStructures[tableName]) {
            case StructureType::AVL:
                avlTables[tableName].insert(rec);
                return true;
            case StructureType::BST:
                bstTables[tableName].insert(rec);
                return true;
            case StructureType::HASH:
                hashTables[tableName].insert(rec);
                return true;
            case StructureType::HEAP:
            default:
                // Original Heap Logic
                // validate schema matches
                auto colsOpt = readMetaFile(tableName);
                if (!colsOpt.has_value()) return false;
                vector<Column> cols = colsOpt.value();

                if (cols.empty()) {
                    // fallback: accept any (legacy)
                } else {
                    if (rec.fields.size() != cols.size()) return false;
                    // require first column is int (primary key)
                    if (!holds_alternative<int>(rec.fields[0])) return false;
                    // types match
                    for (size_t i = 0; i < cols.size(); ++i) {
                        if (!typeStringMatchesValue(cols[i].type, rec.fields[i])) return false;
                    }
                }

                // load all records, remove existing with same id (upsert behaviour)
                vector<Record> records = loadAllRecords(tableName);
                if (rec.fields.size() > 0 && holds_alternative<int>(rec.fields[0])) {
                    int id = get<int>(rec.fields[0]);
                    records.erase(
                        remove_if(records.begin(), records.end(),
                            [&](const Record& r){ return get<int>(r.fields[0]) == id; }),
                        records.end()
                    );
                }

                records.push_back(rec);

                // write all records back (pack into pages)
                ofstream out(tableDataPath(tableName), ios::binary | ios::trunc);
                if (!out) return false;

                Page p;
                p.pageID = 0;
                for (const auto& r : records) {
                    vector<uint8_t> bytes;
                    serializeRecord(r, bytes);
                    auto optSlot = p.insertRawRecord(bytes);
                    if (!optSlot.has_value()) {
                        vector<uint8_t> buffer; p.serializeToBuffer(buffer);
                        out.write((char*)buffer.data(), buffer.size());
                        p = Page(); p.pageID++;
                        p.insertRawRecord(bytes);
                    }
                }
                vector<uint8_t> buffer; p.serializeToBuffer(buffer);
                out.write((char*)buffer.data(), buffer.size());
                out.close();
                return true;
        }
    }

    bool StorageEngine::updateRecord(const string& tableName, int id, const Record& newRecord) {
        auto colsOpt = readMetaFile(tableName);
        if (!colsOpt.has_value()) return false;
        vector<Column> cols = colsOpt.value();

        if (!cols.empty()) {
            if (newRecord.fields.size() != cols.size()) return false;
            // types match
            for (size_t i = 0; i < cols.size(); ++i) {
                if (!typeStringMatchesValue(cols[i].type, newRecord.fields[i])) return false;
            }
        }

        vector<Record> records = loadAllRecords(tableName);
        bool updated = false;
        for (auto& r : records) {
            if (get<int>(r.fields[0]) == id) { r = newRecord; updated = true; break; }
        }
        if (!updated) return false;

        // write back
        ofstream out(tableDataPath(tableName), ios::binary | ios::trunc);
        if (!out) return false;

        Page p;
        p.pageID = 0;
        for (const auto& rec : records) {
            vector<uint8_t> bytes;
            serializeRecord(rec, bytes);
            auto optSlot = p.insertRawRecord(bytes);
            if (!optSlot.has_value()) {
                vector<uint8_t> buffer; p.serializeToBuffer(buffer);
                out.write((char*)buffer.data(), buffer.size());
                p = Page();
                p.pageID++;
                p.insertRawRecord(bytes);
            }
        }
        vector<uint8_t> buffer; p.serializeToBuffer(buffer);
        out.write((char*)buffer.data(), buffer.size());
        out.close();
        return true;
    }

    bool StorageEngine::deleteRecord(const string& tableName, int id) {
        vector<Record> records = loadAllRecords(tableName);

        size_t before = records.size();

        records.erase(
            remove_if(records.begin(), records.end(),
                [&](const Record& r) { return get<int>(r.fields[0]) == id; }),
            records.end()
        );

        if (records.size() == before) return false;  // not found

        ofstream out(tableDataPath(tableName), ios::binary | ios::trunc);
        if (!out) return false;

        Page p;
        p.pageID = 0;
        for (const auto& rec : records) {
            vector<uint8_t> bytes;
            serializeRecord(rec, bytes);
            auto optSlot = p.insertRawRecord(bytes);
            if (!optSlot.has_value()) {
                vector<uint8_t> buffer; p.serializeToBuffer(buffer);
                out.write((char*)buffer.data(), buffer.size());
                p = Page(); p.pageID++;
                p.insertRawRecord(bytes);
            }
        }
        vector<uint8_t> buffer; p.serializeToBuffer(buffer);
        out.write((char*)buffer.data(), buffer.size());
        out.close();

        return true;
    }

    vector<Record> StorageEngine::selectAll(const string& tableName) {
        if (tableStructures.find(tableName) == tableStructures.end()) {
             if (readMetaFile(tableName).has_value()) tableStructures[tableName] = StructureType::HEAP;
             else return {};
        }

        switch (tableStructures[tableName]) {
            case StructureType::AVL:
                return avlTables[tableName].getAllSorted();
            case StructureType::BST:
                return bstTables[tableName].getAllSorted();
            case StructureType::HASH:
                return hashTables[tableName].getAll();
            case StructureType::HEAP:
            default:
                vector<Record> outRecords;
                uint32_t pages = pageCount(tableName);
                for (uint32_t i = 0; i < pages; ++i) {
                    Page p; readPageFromFile(tableName, i, p);
                    for (uint16_t s = 0; s < p.slots.size(); ++s) {
                        if (!p.slots[s].active) continue;
                        vector<uint8_t> raw; p.readRawRecord(s, raw);
                        Record rec; if (deserializeRecord(raw, rec)) outRecords.push_back(move(rec));
                    }
                }
                return outRecords;
        }
    }

    // Helper method to load all records from a table (used by update/delete to avoid redundancy)
    vector<Record> StorageEngine::loadAllRecords(const string& tableName) const {
        vector<Record> records;
        string path = tableDataPath(tableName);

        ifstream in(path, ios::binary);
        if (!in) return records;

        while (true) {
            vector<uint8_t> buffer(PAGE_SIZE);
            if (!in.read((char*)buffer.data(), buffer.size())) break;

            Page p;
            p.deserializeFromBuffer(buffer);

            if (p.slotCount == 0) break;

            for (uint16_t s = 0; s < p.slots.size(); ++s) {
                if (!p.slots[s].active) continue;
                vector<uint8_t> raw;
                if (p.readRawRecord(s, raw)) {
                    Record rec;
                    if (deserializeRecord(raw, rec)) {
                        records.push_back(rec);
                    }
                }
            }
        }
        in.close();
        return records;
    }

    // --- Meta file helpers ---
    // meta format: columns=col1:TYPE,col2:TYPE,col3:TYPE
    bool StorageEngine::writeMetaFile(const string& tableName, const vector<Column>& columns) const {
        string path = tableMetaPath(tableName);
        ofstream m(path, ios::trunc);
        if (!m) return false;

        m << "table=" << tableName << "\n";
        m << "columns=";
        for (size_t i = 0; i < columns.size(); ++i) {
            m << columns[i].name << ":" << columns[i].type;
            if (i + 1 < columns.size()) m << ",";
        }
        m << "\n";
        m.close();
        return true;
    }

    optional<vector<Column>> StorageEngine::readMetaFile(const string& tableName) const {
        string path = tableMetaPath(tableName);
        if (!fs::exists(path)) return nullopt;

        ifstream m(path);
        if (!m) return nullopt;

        string line;
        vector<Column> cols;
        while (getline(m, line)) {
            // trim
            if (line.rfind("columns=", 0) == 0) {
                string rest = line.substr(strlen("columns="));
                // split by ','
                stringstream ss(rest);
                string token;
                while (getline(ss, token, ',')) {
                    // token like name:TYPE
                    size_t pos = token.find(':');
                    if (pos == string::npos) continue;
                    string cname = token.substr(0, pos);
                    string ctype = token.substr(pos + 1);
                    // trim spaces
                    auto trim = [](string s)->string{
                        size_t a = 0;
                        while (a < s.size() && isspace((unsigned char)s[a])) a++;
                        size_t b = s.size();
                        while (b> a && isspace((unsigned char)s[b-1])) b--;
                        return s.substr(a, b-a);
                    };
                    cname = trim(cname);
                    ctype = trim(ctype);
                    cols.push_back({cname, ctype});
                }
            }
        }
        m.close();
        return cols;
    }

    vector<Column> StorageEngine::getTableColumns(const string& tableName) const {
        auto opt = readMetaFile(tableName);
        if (!opt.has_value()) return {};
        return opt.value();
    }

    StorageEngine::StructureType StorageEngine::getStructureType(const string& tableName) const {
        if (tableStructures.find(tableName) != tableStructures.end()) {
            return tableStructures.at(tableName);
        }
        return StructureType::HEAP;
    }

} // namespace ChronoDB
