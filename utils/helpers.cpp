//Utility functions (print, tokenize, format)
#include "helpers.h"
#include<algorithm>
#include<sstream>
#include<iostream>

using namespace std;

namespace Helper{
    string trim(const string& str) {
        int start = 0;
        while (start < (int)str.size() && isspace(str[start])) {
            start++;
        }
        int end = str.size() - 1;
        while (end >= start && isspace(str[end])) {
            end--;
        }
        if(start>end) return "";
        return str.substr(start, end - start + 1);
    }
    vector<string> split(const string& str, char delimiter) {
        vector<string> tokens;
        string token;
        istringstream tokenStream(str);
        while (getline(tokenStream, token, delimiter)){
            if(!token.empty()) tokens.push_back(trim(token));
        }
        return tokens;
    }
    bool isNumber(const string& str){
        if(str.empty()) return false;
        return all_of(str.begin(), str.end(), ::isdigit);
    }
    string toUpper(const string& str){
        string result = str;
        transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }
    void printError(const string& message){
        cout << "[ERROR]: " << message << endl;
    }
    void printSuccess(const string& message){
        cout << "[SUCCESS]: " << message << endl;
    }
    void printRecord(const vector<string>& fields){
        for(const auto& field : fields){
            cout << field << " | ";
        }
        cout << endl;
    }
    void printLine(char ch, int count){
        for(int i=0;i<count;i++){
            cout << ch;
        }
        cout << endl;
    }
}