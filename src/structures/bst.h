#ifndef CHRONODB_STRUCTURES_BST_H
#define CHRONODB_STRUCTURES_BST_H

#include "../../utils/types.h"
#include <iostream>
#include <queue>
#include <stack>
#include <vector>
#include <optional>

namespace ChronoDB {

    struct BSTNode {
        int id;             // Key
        Record data;        // Value
        BSTNode* left = nullptr;
        BSTNode* right = nullptr;

        BSTNode(int _id, const Record& _data) : id(_id), data(_data) {}
    };

    class BST {
    private:
        BSTNode* root = nullptr;

        void insertHelper(BSTNode*& node, int id, const Record& rec) {
            if (!node) {
                node = new BSTNode(id, rec);
                return;
            }
            if (id < node->id) insertHelper(node->left, id, rec);
            else insertHelper(node->right, id, rec);
        }

        void inOrderHelper(BSTNode* node, std::vector<Record>& results) const {
            if (!node) return;
            inOrderHelper(node->left, results);
            results.push_back(node->data);
            inOrderHelper(node->right, results);
        }
        
        void clearHelper(BSTNode* node) {
            if (!node) return;
            clearHelper(node->left);
            clearHelper(node->right);
            delete node;
        }

    public:
        BST() = default;
        ~BST() { clearHelper(root); }

        void insert(const Record& rec) {
            // Assume ID is the first field (Index 0) and is INT
            if (rec.fields.empty()) return;
            try {
                int id = std::get<int>(rec.fields[0]);
                insertHelper(root, id, rec);
            } catch (...) {
                std::cerr << "Error: Primary Key must be INT for BST." << std::endl;
            }
        }

        // Standard Binary Search (Recursive)
        std::optional<Record> search(int id) const {
            BSTNode* current = root;
            while (current) {
                if (id == current->id) return current->data;
                else if (id < current->id) current = current->left;
                else current = current->right;
            }
            return std::nullopt;
        }

        // --- ALGORITHMS ---

        // BFS: Breadth-First Search (Level Order)
        std::optional<Record> searchBFS(int targetID) const {
            if (!root) return std::nullopt;

            std::queue<BSTNode*> q;
            q.push(root);

            std::cout << "[BFS Traversal]: "; 

            while (!q.empty()) {
                BSTNode* current = q.front();
                q.pop();

                std::cout << current->id << " "; // Visited

                if (current->id == targetID) {
                    std::cout << "(Found!)" << std::endl;
                    return current->data;
                }

                if (current->left) q.push(current->left);
                if (current->right) q.push(current->right);
            }
            std::cout << "(Not Found)" << std::endl;
            return std::nullopt;
        }

        // DFS: Depth-First Search (Pre-Order using Stack)
        std::optional<Record> searchDFS(int targetID) const {
            if (!root) return std::nullopt;

            std::stack<BSTNode*> s;
            s.push(root);

            std::cout << "[DFS Traversal]: ";

            while (!s.empty()) {
                BSTNode* current = s.top();
                s.pop();

                std::cout << current->id << " "; // Visited

                if (current->id == targetID) {
                    std::cout << "(Found!)" << std::endl;
                    return current->data;
                }

                // Push right then left so left is processed first
                if (current->right) s.push(current->right);
                if (current->left) s.push(current->left);
            }
            std::cout << "(Not Found)" << std::endl;
            return std::nullopt;
        }

        std::vector<Record> getAllSorted() const {
            std::vector<Record> results;
            inOrderHelper(root, results);
            return results;
        }
    };

} // namespace ChronoDB

#endif
