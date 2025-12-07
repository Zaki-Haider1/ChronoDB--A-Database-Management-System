#include "storage.h"
#include <filesystem>
#include <cstring>
#include <iostream>
#include <algorithm>
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
    }

    StorageEngine::~StorageEngine() {}

    string StorageEngine::tableDataPath(const string& tableName) const {
        return storageDirectory + "/" + tableName + ".tbl";
    }

    string StorageEngine::tableMetaPath(const string& tableName) const {
        return storageDirectory + "/" + tableName + ".meta";
    }

    bool StorageEngine::createTable(const string& tableName) {
    string path = tableDataPath(tableName);
    if (fs::exists(path)) return false;

    ofstream out(path, ios::binary);
    Page p;
    p.pageID = 0;

    vector<uint8_t> buffer;  // lvalue
    p.serializeToBuffer(buffer);  // pass the named vector
    out.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
    out.close();

    ofstream meta(tableMetaPath(tableName));
    if (meta) { 
        meta << "table=" << tableName << "\n"; 
        meta.close(); 
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

    bool StorageEngine::insertRecord(const string& tableName, const Record& rec) {
        vector<uint8_t> bytes; serializeRecord(rec, bytes);
        if (bytes.size() + sizeof(SlotEntry) > PAGE_SIZE - PAGE_HEADER_RESERVED) return false;

        uint32_t pages = pageCount(tableName);
        if (pages == 0) appendEmptyPage(tableName);

        Page p;
        readPageFromFile(tableName, pages - 1, p);
        auto optSlot = p.insertRawRecord(bytes);
        if (!optSlot.has_value()) {
            uint32_t newPageIdx = appendEmptyPage(tableName);
            readPageFromFile(tableName, newPageIdx, p);
            optSlot = p.insertRawRecord(bytes);
            if (!optSlot.has_value()) return false;
            writePageToFile(tableName, newPageIdx, p);
            return true;
        }
        writePageToFile(tableName, pages - 1, p);
        return true;
    }

    bool StorageEngine::updateRecord(const string& tableName, int id, const Record& newRecord) {
        string path = tableDataPath(tableName);

        ifstream in(path, ios::binary);
        if (!in) return false;

        vector<Record> records;
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

        bool updated = false;
        for (auto& r : records) {
            if (get<int>(r.fields[0]) == id) {  // ID is first field
                r = newRecord;
                updated = true;
                break;
            }
        }
        if (!updated) return false;

        ofstream out(path, ios::binary | ios::trunc);
        for (const auto& rec : records) {
            vector<uint8_t> bytes;
            serializeRecord(rec, bytes);
            Page newPage;
            newPage.pageID = 0;
            auto optSlot = newPage.insertRawRecord(bytes);
            if (optSlot.has_value()) {
                vector<uint8_t> buffer;
                newPage.serializeToBuffer(buffer);
                out.write((char*)buffer.data(), buffer.size());
            }
        }
        out.close();

        return true;
    }

    bool StorageEngine::deleteRecord(const string& tableName, int id) {
        string path = tableDataPath(tableName);

        ifstream in(path, ios::binary);
        if (!in) return false;

        vector<Record> records;
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

        int before = records.size();

        records.erase(
            remove_if(records.begin(), records.end(),
                [&](const Record& r) { return get<int>(r.fields[0]) == id; }),
            records.end()
        );

        if (records.size() == before) return false;  // not found

        ofstream out(path, ios::binary | ios::trunc);
        out.close();

        return true;
    }

    vector<Record> StorageEngine::selectAll(const string& tableName) {
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

} // namespace ChronoDB
