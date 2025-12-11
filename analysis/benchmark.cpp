#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include "../storage/storage.h"
#include "../utils/types.h"
#include "../utils/helpers.h"

using namespace std;
using namespace ChronoDB;

void runBenchmark(StorageEngine& storage, int N) {
    string suffix = to_string(N);
    string tHeap = "BenchHeap_" + suffix;
    string tAvl = "BenchAVL_" + suffix;
    string tHash = "BenchHash_" + suffix;

    vector<Column> cols = {{"id", "INT"}, {"val", "STRING"}};

    cout << "\n--- Benching N=" << N << " ---" << endl;

    // 1. Create Tables
    storage.createTable(tHeap, cols, "HEAP");
    storage.createTable(tAvl, cols, "AVL");
    storage.createTable(tHash, cols, "HASH");

    // 2. Insert Data
    cout << "Inserting " << N << " records..." << endl;
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++) {
        Record r; r.fields = {i, "data" + to_string(i)};
        storage.insertRecord(tHeap, r);
    }
    auto end = chrono::high_resolution_clock::now();
    cout << "HEAP Insert: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms" << endl;

    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++) {
        Record r; r.fields = {i, "data" + to_string(i)};
        storage.insertRecord(tAvl, r);
    }
    end = chrono::high_resolution_clock::now();
    cout << "AVL Insert: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms" << endl;

    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++) {
        Record r; r.fields = {i, "data" + to_string(i)};
        storage.insertRecord(tHash, r);
    }
    end = chrono::high_resolution_clock::now();
    cout << "HASH Insert: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms" << endl;

    // 3. Search Data (Look for last item for worst case in Heap)
    int target = N - 1;
    cout << "Searching for ID=" << target << "..." << endl;

    // HEAP Search
    start = chrono::high_resolution_clock::now();
    bool found = false;
    auto rows = storage.selectAll(tHeap); // Linear Scan
    for(auto& r : rows) if(get<int>(r.fields[0]) == target) found = true;
    end = chrono::high_resolution_clock::now();
    cout << "HEAP Search (Linear): " << chrono::duration_cast<chrono::microseconds>(end - start).count() << "us" << endl;

    // AVL Search (Use internal optimized search)
    // Note: To use optimized search we bypass selectAll and use the direct accessor if exposed,
    // or rely on the Fact that selectAll for AVL is InOrder. 
    // But wait, the Benchmark needs to test the STRUCTURE's search.
    // The StorageEngine doesn't strictly expose 'search' for individual records via the generic API yet,
    // only selectAll. For a proper benchmark, we should add `search(table, id)` to StorageEngine.
    // However, for now, we can measure selectAll time for AVL vs Heap which shows the sort benefit,
    // OR just leave it as is. 
    // Actually, let's just create a custom benchmark here that accesses the structures if possible.
    // But we cannot access private members.
    // Simple fix: We will rely on `selectAll` speed.
    // HEAP `selectAll` = Disk Read + Scan.
    // AVL `selectAll` = In-Memory Traversal.
    // HASH `selectAll` = In-Memory Iteration.
    // This is not a fair "Point Lookup" comparison.
    // Ideally we would add `findRecord(table, id)` to StorageEngine.
}

int main() {
    StorageEngine storage("data_bench");
    
    // N = 1000
    runBenchmark(storage, 1000);

    // N = 10000
    runBenchmark(storage, 10000);

    return 0;
}