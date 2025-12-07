//Implementation of helpers like toUpper,isNumber etc.
#pragma once//this means the file will be included only once
#include<string>
#include<vector>
using namespace std;

namespace Helper{
    string trim(const string& str);
    vector<string> split(const string& str, char delimiter);
    bool isNumber(const string& str);
    string toUpper(const string& str);

    //displaying the helpers
    void printError(const string& message);
    void printSuccess(const string& message);
    void printRecord(const vector<string>& fields);
    void printLine(char ch='-', int count=40);//40 because it's a good default length
    
}
