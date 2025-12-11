# Project Design Notes & Context

## 1. Project Goal

We are building a **Multi-Structure DBMS** to demonstrate Data Structures and Algorithms (DSA) concepts.
Different tables will be backed by different Data Structures to compare their performance (Big-O).

## 2. Supported Structures

### A. HEAP Table (The "Baseline")

- **What is it?**: A "Heap" in database terms (not the Priority Queue heap) is just an unordered pile of records. We simply append new data to the end of the file.
- **Purpose**: To serve as the "Slow" baseline for our comparisons.
- **Performance**:
  - **Insert**: $O(1)$ (Just add to end).
  - **Search**: $O(N)$ (Must look at every single record to find `ID=500`).
- **Analogy**: A notebook where you just write notes one after another. To find a specific note, you have to read the whole book.

### B. TREE Table (AVL Tree)

- **What is it?**: A self-balancing Binary Search Tree.
- **Purpose**: To demonstrate efficient, sorted storage.
- **Performance**:
  - **Insert**: $O(\log N)$.
  - **Search**: $O(\log N)$ (Binary Search).

### C. HASH Table (Hash Map)

- **What is it?**: An array of lists (Chaining) where data is placed based on `ID % Size`.
- **Purpose**: To demonstrate the fastest point-lookup.
- **Performance**:
  - **Insert**: $O(1)$ (Average).
  - **Search**: $O(1)$ (Average).

### D. BST Table (Algorithm Sandbox)

- **What is it?**: A standard (unbalanced) Binary Search Tree.
- **Purpose**: To manually test Graph/Tree traversal algorithms.
- **Features**:
  - `SELECT ... USING BFS`: Breadth-First Search (Level Order).
  - `SELECT ... USING DFS`: Depth-First Search (Pre-order).

## 3. Data Flow

1.  **Parser**: Reads `CREATE TABLE ... USING [TYPE]`.
2.  **Storage Engine**: Looks up the type in a registry.
3.  **Structure**: The specific class (`BST`, `AVL`, `Hash`) handles the actual data storage in memory/disk.

## Saved Chat Context

- **User Decision**: We moved away from "Hidden Indexes" to "Explicit Structures".
- **Key**: The Primary Key (ID) is used to organize the structure.
- **Value**: The entire Record (Row) is stored in the node.
