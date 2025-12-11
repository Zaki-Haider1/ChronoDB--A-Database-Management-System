#ifndef CHRONODB_STRUCTURES_HASH_H
#define CHRONODB_STRUCTURES_HASH_H

#include "../../utils/types.h"
#include <list>
#include <vector>
#include <optional>
#include <iostream>

namespace ChronoDB {

    struct HashNode {
        int id;
        Record data;
        HashNode(int _id, const Record& _data) : id(_id), data(_data) {}
    };

    class HashTable {
    private:
        static const int TABLE_SIZE = 1009; // Prime number for better distribution
        std::vector<std::list<HashNode>> table;

        int hashFunction(int id) const {
            return id % TABLE_SIZE;
        }

    public:
        HashTable() {
            table.resize(TABLE_SIZE);
        }

        void insert(const Record& rec) {
            if (rec.fields.empty()) return;
            try {
                int id = std::get<int>(rec.fields[0]);
                int index = hashFunction(id);
                table[index].emplace_back(id, rec);
            } catch (...) {
                std::cerr << "Error: Primary Key must be INT for HashTable." << std::endl;
            }
        }

        std::optional<Record> search(int id) const {
            int index = hashFunction(id);
            const auto& chain = table[index];

            for (const auto& node : chain) {
                if (node.id == id) {
                    return node.data;
                }
            }
            return std::nullopt;
        }

        std::vector<Record> getAll() const {
            std::vector<Record> results;
            for (const auto& chain : table) {
                for (const auto& node : chain) {
                    results.push_back(node.data);
                }
            }
            return results;
        }
    };

} // namespace ChronoDB

#endif
