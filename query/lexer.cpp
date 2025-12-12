#include "lexer.h"
#include <cctype>
#include <unordered_set>

using namespace std;

namespace ChronoDB {

    Lexer::Lexer(string input) : src(move(input)) {}

    char Lexer::current() {
        if (pos >= src.size()) return '\0';
        return src[pos];
    }

    void Lexer::advance() { pos++; }

    void Lexer::skipWhitespace() {
        while (isspace(current())) advance();
    }

    Token Lexer::readString() {
        advance(); 
        string val;
        while (current() != '"' && current() != '\0') {
            val += current();
            advance();
        }
        advance(); 
        return {TokenType::STRING_LITERAL, val};
    }

    Token Lexer::readNumber() {
        string val;
        while (isdigit(current()) || current() == '.') {
            val += current();
            advance();
        }
        return {TokenType::NUMBER, val};
    }

    Token Lexer::readIdentifierOrKeyword() {
        string val;
        while (isalnum(current()) || current() == '_') {
            val += current();
            advance();
        }
        return {TokenType::IDENTIFIER, val};
    }

    Token Lexer::nextToken() {
        skipWhitespace();
        if (current() == '\0') return {TokenType::END_OF_FILE, ""};

        if (isalpha(current())) return readIdentifierOrKeyword();
        if (isdigit(current())) return readNumber();
        if (current() == '"') return readString();

        char c = current();
        advance();

        if ((c == '=' || c == '!' || c == '<' || c == '>') && current() == '=') {
            string sym(1, c);
            sym += '=';
            advance();
            return {TokenType::SYMBOL, sym};
        }

        string sym(1, c);
        return {TokenType::SYMBOL, sym};
    }

    vector<Token> Lexer::tokenize() {
        vector<Token> tokens;
        Token t = nextToken();
        while (t.type != TokenType::END_OF_FILE) {
            tokens.push_back(t);
            t = nextToken();
        }
        return tokens;
    }
}
