#ifndef CHRONODB_STRUCTURES_AVL_H
#define CHRONODB_STRUCTURES_AVL_H

#include "../../utils/types.h"
#include <algorithm>
#include <vector>
#include <optional>
#include <iostream>

namespace ChronoDB {

    struct AVLNode {
        int id;
        Record data;
        AVLNode* left = nullptr;
        AVLNode* right = nullptr;
        int height = 1;

        AVLNode(int _id, const Record& _data) : id(_id), data(_data) {}
    };

    class AVLTree {
    private:
        AVLNode* root = nullptr;

        int height(AVLNode* N) {
            return (N == nullptr) ? 0 : N->height;
        }

        int getBalance(AVLNode* N) {
            return (N == nullptr) ? 0 : height(N->left) - height(N->right);
        }

        AVLNode* rightRotate(AVLNode* y) {
            AVLNode* x = y->left;
            AVLNode* T2 = x->right;

            x->right = y;
            y->left = T2;

            y->height = std::max(height(y->left), height(y->right)) + 1;
            x->height = std::max(height(x->left), height(x->right)) + 1;

            return x;
        }

        AVLNode* leftRotate(AVLNode* x) {
            AVLNode* y = x->right;
            AVLNode* T2 = y->left;

            y->left = x;
            x->right = T2;

            x->height = std::max(height(x->left), height(x->right)) + 1;
            y->height = std::max(height(y->left), height(y->right)) + 1;

            return y;
        }

        AVLNode* insertHelper(AVLNode* node, int id, const Record& rec) {
            if (node == nullptr)
                return new AVLNode(id, rec);

            if (id < node->id)
                node->left = insertHelper(node->left, id, rec);
            else if (id > node->id)
                node->right = insertHelper(node->right, id, rec);
            else
                return node; // No duplicates allowed for now

            node->height = 1 + std::max(height(node->left), height(node->right));

            int balance = getBalance(node);

            // Left Left Case
            if (balance > 1 && id < node->left->id)
                return rightRotate(node);

            // Right Right Case
            if (balance < -1 && id > node->right->id)
                return leftRotate(node);

            // Left Right Case
            if (balance > 1 && id > node->left->id) {
                node->left = leftRotate(node->left);
                return rightRotate(node);
            }

            // Right Left Case
            if (balance < -1 && id < node->right->id) {
                node->right = rightRotate(node->right);
                return leftRotate(node);
            }

            return node;
        }
        
        void inOrderHelper(AVLNode* node, std::vector<Record>& results) const {
            if (!node) return;
            inOrderHelper(node->left, results);
            results.push_back(node->data);
            inOrderHelper(node->right, results);
        }

        void clearHelper(AVLNode* node) {
            if (!node) return;
            clearHelper(node->left);
            clearHelper(node->right);
            delete node;
        }

    public:
        AVLTree() = default;
        ~AVLTree() { clearHelper(root); }

        void insert(const Record& rec) {
            if (rec.fields.empty()) return;
            try {
                int id = std::get<int>(rec.fields[0]);
                root = insertHelper(root, id, rec);
            } catch (...) {
                std::cerr << "Error: Primary Key must be INT for AVL." << std::endl;
            }
        }

        std::optional<Record> search(int id) const {
            AVLNode* current = root;
            while (current) {
                if (id == current->id) return current->data;
                else if (id < current->id) current = current->left;
                else current = current->right;
            }
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
