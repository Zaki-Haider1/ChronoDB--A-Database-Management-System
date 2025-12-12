#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include "../storage/storage.h"
#include "../utils/types.h"
#include "../utils/helpers.h"
#include "../utils/sorting.h"

using namespace std;
using namespace ChronoDB;

void runBenchmark(StorageEngine& storage, int N) {
    string suffix = to_string(N);
    string tHeap = "BenchHeap_" + suffix;
    string tAvl = "BenchAVL_" + suffix;
    string tHash = "BenchHash_" + suffix;

    vector<Column> cols = {{"id", "INT"}, {"val", "STRING"}};

    cout << "\n==========================================" << endl;
    cout << "   BENCHMARK SUITE (N=" << N << ")" << endl;
    cout << "==========================================" << endl;

    // -------------------------------------------------
    // 1. CREATE TABLES
    // -------------------------------------------------
    storage.createTable(tHeap, cols, "HEAP");
    storage.createTable(tAvl, cols, "AVL");
    storage.createTable(tHash, cols, "HASH");

    // -------------------------------------------------
    // 2. INSERTION TEST
    // -------------------------------------------------
    cout << "\n[INSERTION] Inserting " << N << " records..." << endl;
    
    // HEAP
    auto start = chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++) {
        Record r; r.fields = {i, "data" + to_string(i)};
        storage.insertRecord(tHeap, r);
    }
    auto end = chrono::high_resolution_clock::now();
    cout << "  HEAP: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms" << endl;

    // AVL
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++) {
        Record r; r.fields = {i, "data" + to_string(i)};
        storage.insertRecord(tAvl, r);
    }
    end = chrono::high_resolution_clock::now();
    cout << "  AVL : " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms" << endl;

    // HASH
    start = chrono::high_resolution_clock::now();
    for (int i = 0; i < N; i++) {
        Record r; r.fields = {i, "data" + to_string(i)};
        storage.insertRecord(tHash, r);
    }
    end = chrono::high_resolution_clock::now();
    cout << "  HASH: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms" << endl;

    // -------------------------------------------------
    // 3. POINT SEARCH TEST (Find ID = N-1)
    // -------------------------------------------------
    int target = N - 1;
    cout << "\n[POINT SEARCH] Looking for ID=" << target << "..." << endl;

    // HEAP (Linear Scan via StorageEngine::search)
    start = chrono::high_resolution_clock::now();
    storage.search(tHeap, target);
    end = chrono::high_resolution_clock::now();
    cout << "  HEAP (Scan)    : " << chrono::duration_cast<chrono::microseconds>(end - start).count() << "us" << endl;

    // AVL (Tree Search)
    start = chrono::high_resolution_clock::now();
    storage.search(tAvl, target);
    end = chrono::high_resolution_clock::now();
    cout << "  AVL  (Height)  : " << chrono::duration_cast<chrono::microseconds>(end - start).count() << "us" << endl;

    // HASH (Direct Lookup)
    start = chrono::high_resolution_clock::now();
    storage.search(tHash, target);
    end = chrono::high_resolution_clock::now();
    cout << "  HASH (Direct)  : " << chrono::duration_cast<chrono::microseconds>(end - start).count() << "us" << endl;

    // -------------------------------------------------
    // 4. RANGE SEARCH TEST (ID > N/2)
    // -------------------------------------------------
    // We will compare:
    // A. Linear Scan (manual iteration)
    // B. Sort + Binary Search (using Utils::Sorting)
    cout << "\n[RANGE SEARCH] Query: ID > " << (N/2) << "..." << endl;

    // Fetch data first (Disk I/O is common to both, but we can include it or exclude it. 
    // To measure Algo, let's load first.)
    auto rows = storage.selectAll(tHeap); 
    
    // A. Linear Scan
    start = chrono::high_resolution_clock::now();
    int countScan = 0;
    for(auto& r : rows) {
        if (get<int>(r.fields[0]) > (N/2)) countScan++;
    }
    end = chrono::high_resolution_clock::now();
    auto timeScan = chrono::duration_cast<chrono::microseconds>(end - start).count();
    cout << "  Linear Scan    : " << timeScan << "us (Count: " << countScan << ")" << endl;

    // B. Sort + Binary Search
    // We restart with fresh rows to simulate the process
    auto rows2 = rows; 
    start = chrono::high_resolution_clock::now();
    
    // Sort
    Sorting::mergeSort(rows2, 0, "INT");
    
    // Binary Search
    int cutIndex = Sorting::binarySearchUpperBound(rows2, 0, "INT", to_string(N/2));
    
    // "Fetch" results (iterate from cutIndex)
    int countSort = 0;
    for(size_t i = cutIndex; i < rows2.size(); i++) {
        countSort++;
    }
    
    end = chrono::high_resolution_clock::now();
    auto timeSort = chrono::duration_cast<chrono::microseconds>(end - start).count();
    cout << "  Sort + Search  : " << timeSort << "us (Count: " << countSort << ")" << endl;

}

int main() {
    // Use a separate directory for benchmarking to avoid polluting main data
    // Warning: StorageEngine constructor might not support custom paths easily if hardcoded in some places, 
    // but the one in storage.cpp/h uses a baseDir default "data" if not specified? 
    // Checking main.cpp... StorageEngine storage; -> uses "data".
    // Checking benchmark.cpp previous -> StorageEngine storage("data_bench");
    
    // Ensure "data_bench" exists or is handled.
    // The previous benchmark code used "data_bench".
    
    // Clean up previous bench data if possible?
    // Not implementing recursive delete here, assume StorageEngine handles or we append.
    // Ideally we'd delete the folder system command but let's just run.

    StorageEngine storage("analysis_data"); // Distinct folder
    
    // N = 1,000
    runBenchmark(storage, 1000);

    // N = 10,000
    runBenchmark(storage, 10000);

    // N = 100,000 (Requirement: at least 3 input sizes)
    runBenchmark(storage, 100000);

    return 0;
}