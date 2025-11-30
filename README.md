ChronoDB-A Database Management System

ChronoDB is a custom C++ DBMS implementing a multi-module architecture:

- **Storage Engine**: CRUD, tables, persistence, undo/redo operations
- **Query Engine**: ChronaQL parser with lexer -> AST -> executor
- **Indexing Engine**: BST/AVL/B-Tree for fast lookups
- **Graph Engine**: DFS, BFS, Dijkstra, optional A* with CLI visualization
- **Transactions Module**: BEGIN, COMMIT, ROLLBACK, locking mechanism
- **Benchmark Module**: Sorting, searching, graph performance analysis with Big-O estimates
- **Testing**: Unit tests and performance validation

This project demonstrates advanced **data structures and algorithms**, professional software practices, and a real-world DBMS workflow. Suitable for academic assessment and CV showcase.
