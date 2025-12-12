#include "sorting.h"
#include <iostream>
#include <cmath>
#include "helpers.h"

using namespace std;

namespace ChronoDB {

    bool Sorting::compare(const Record& a, const Record& b, int colIndex, const string& colType) {
        if (colType == "INT") {
            return get<int>(a.fields[colIndex]) < get<int>(b.fields[colIndex]);
        } else if (colType == "FLOAT") {
            return get<float>(a.fields[colIndex]) < get<float>(b.fields[colIndex]);
        } else {
            return get<string>(a.fields[colIndex]) < get<string>(b.fields[colIndex]);
        }
    }

    bool Sorting::compareVal(const Record& a, const string& bVal, int colIndex, const string& colType) {
        if (colType == "INT") {
            return get<int>(a.fields[colIndex]) < stoi(bVal);
        } else if (colType == "FLOAT") {
            return get<float>(a.fields[colIndex]) < stof(bVal);
        } else {
            return get<string>(a.fields[colIndex]) < bVal;
        }
    }

    void Sorting::merge(vector<Record>& rows, int left, int mid, int right, int colIndex, const string& colType) {
        int n1 = mid - left + 1;
        int n2 = right - mid;

        vector<Record> L(n1), R(n2);

        for (int i = 0; i < n1; i++) L[i] = rows[left + i];
        for (int j = 0; j < n2; j++) R[j] = rows[mid + 1 + j];

        int i = 0, j = 0, k = left;
        while (i < n1 && j < n2) {
            if (compare(L[i], R[j], colIndex, colType) || 
               (!compare(R[j], L[i], colIndex, colType) && !compare(L[i], R[j], colIndex, colType))) { 
                // L[i] <= R[j] (stableish)
                rows[k] = L[i];
                i++;
            } else {
                rows[k] = R[j];
                j++;
            }
            k++;
        }

        while (i < n1) {
            rows[k] = L[i];
            i++;
            k++;
        }

        while (j < n2) {
            rows[k] = R[j];
            j++;
            k++;
        }
    }

    void Sorting::mergeSortRecursive(vector<Record>& rows, int left, int right, int colIndex, const string& colType) {
        if (left >= right) return;
        int mid = left + (right - left) / 2;
        mergeSortRecursive(rows, left, mid, colIndex, colType);
        mergeSortRecursive(rows, mid + 1, right, colIndex, colType);
        merge(rows, left, mid, right, colIndex, colType);
    }

    void Sorting::mergeSort(vector<Record>& rows, int colIndex, const string& colType) {
        if (rows.empty()) return;
        mergeSortRecursive(rows, 0, rows.size() - 1, colIndex, colType);
    }

    int Sorting::binarySearchLowerBound(const vector<Record>& rows, int colIndex, const string& colType, const string& val) {
        int left = 0, right = rows.size();
        while (left < right) {
            int mid = left + (right - left) / 2;
            if (compareVal(rows[mid], val, colIndex, colType)) {
                left = mid + 1;
            } else {
                right = mid;
            }
        }
        return left;
    }
    
    int Sorting::binarySearchUpperBound(const vector<Record>& rows, int colIndex, const string& colType, const string& val) {
        int left = 0, right = rows.size();
        while (left < right) {
            int mid = left + (right - left) / 2;
            // Check if val < rows[mid]
            // We want first element > val
            // So if rows[mid] <= val, we move left to mid+1
            
            bool isLessOrEqual = false;
            // check if rows[mid] <= val
            // rows[mid] <= val is !(val < rows[mid]) .. wait
            // Let's use compareVal which does rows[mid] < val
            
            // if rows[mid] < val -> true. then it is strictly less. 
            // if rows[mid] == val -> false. 
            // if rows[mid] > val -> false.
            
            // We want first element where rows[mid] > val
            
            // custom check:
            bool rowLessVal = compareVal(rows[mid], val, colIndex, colType);
            bool valLessRow = false;
            
             if (colType == "INT") valLessRow = stoi(val) < get<int>(rows[mid].fields[colIndex]);
             else if (colType == "FLOAT") valLessRow = stof(val) < get<float>(rows[mid].fields[colIndex]);
             else valLessRow = val < get<string>(rows[mid].fields[colIndex]);
             
             if (!valLessRow) { // rows[mid] <= val
                 left = mid + 1;
             } else {
                 right = mid;
             }
        }
        return left;
    }

}
