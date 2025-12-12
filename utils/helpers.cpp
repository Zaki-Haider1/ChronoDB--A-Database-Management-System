#include "helpers.h"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <vector>
#include <variant>
#include <iomanip>

using namespace std;

namespace Helper {

    // Capture Buffer
    static bool isCapturing = false;
    static stringstream captureBuffer;

    void startCapture() {
        isCapturing = true;
        captureBuffer.str("");
        captureBuffer.clear();
    }

    string stopCapture() {
        isCapturing = false;
        return captureBuffer.str();
    }

    string getCaptured() {
        return captureBuffer.str();
    }

    void print(const string& msg) {
        if (isCapturing) {
            captureBuffer << msg;
        } else {
            cout << msg;
        }
    }

    void println(const string& msg) {
        print(msg + "\n");
    }

    string trim(const string& str) {
        int start = 0;
        while (start < (int)str.size() && isspace(str[start])) start++;
        int end = str.size() - 1;
        while (end >= start && isspace(str[end])) end--;
        if (start > end) return "";
        return str.substr(start, end - start + 1);
    }

    vector<string> split(const string& str, char delimiter) {
        vector<string> tokens;
        string token;
        istringstream tokenStream(str);
        while (getline(tokenStream, token, delimiter)) {
            if (!token.empty()) tokens.push_back(trim(token));
        }
        return tokens;
    }

    bool isNumber(const string& str) {
        if (str.empty()) return false;
        return all_of(str.begin(), str.end(), ::isdigit);
    }

    string toUpper(const string& str) {
        string result = str;
        transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }

    void printError(const string& message) {
        if (isCapturing) {
             println("[ERROR]: " + message);
        } else {
             cout << "\033[31m[ERROR]: " << message << "\033[0m" << endl;
        }
    }

    void printSuccess(const string& message) {
        if (isCapturing) {
             println("[SUCCESS]: " + message);
        } else {
             cout << "\033[32m[SUCCESS]: " << message << "\033[0m" << endl;
        }
    }

    void printLine(char ch, int count) {
        stringstream ss;
        for (int i = 0; i < count; i++) ss << ch;
        println(ss.str());
    }

    void printRecord(const vector<string>& fields) {
        stringstream ss;
        for (const auto& field : fields) ss << field << " | ";
        println(ss.str());
    }

    // --- Pretty table printing for SELECT ---
    void printTable(const vector<vector<variant<int, float, string>>>& rows,const vector<string>& headers) {
        if (headers.empty()) return;
        
        vector<size_t> widths(headers.size(), 0);

        // 1. Calculate column widths based on headers
        for (size_t i = 0; i < headers.size(); i++)
            widths[i] = headers[i].size();

        // 2. Calculate column widths based on data
        for (const auto& row : rows) {
            for (size_t i = 0; i < row.size(); i++) {
                string val = visit([](auto&& v) -> string {
                    ostringstream oss;
                    oss << v;
                    return oss.str();
                }, row[i]);
                widths[i] = max(widths[i], val.size());
            }
        }

        // Helper lambda for line
        auto makeLine = [&](char ch) {
            stringstream ss;
            ss << "+";
            for (auto w : widths) ss << string(w + 2, '-') << "+";
            return ss.str();
        };

        // 3. Print Header
        println(makeLine('-'));
        
        stringstream headerLine;
        headerLine << "|";
        for (size_t i = 0; i < headers.size(); i++)
            headerLine << " " << setw(widths[i]) << headers[i] << " |";
        println(headerLine.str());

        println(makeLine('-'));

        // 4. Print Rows
        for (const auto& row : rows) {
            stringstream rowLine;
            rowLine << "|";
            for (size_t i = 0; i < row.size(); i++) {
                string val = visit([](auto&& v) -> string {
                    ostringstream oss;
                    oss << v;
                    return oss.str();
                }, row[i]);
                rowLine << " " << setw(widths[i]) << val << " |";
            }
            println(rowLine.str());
        }

        // 5. Bottom Line
        println(makeLine('-'));
    }

} // namespace Helper
