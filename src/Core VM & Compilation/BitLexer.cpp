#include "BitLexer.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

// ─────────────────────────────────────────────────────────────────────────────
// Lexical Analysis: Source → Tokens
// ─────────────────────────────────────────────────────────────────────────────
// Tokenizer recognizes keywords, identifiers, numbers, strings, and operators.
// ─────────────────────────────────────────────────────────────────────────────

BitLexer::BitLexer(const std::string& src) : src(src), pos(0), line(1) {}

std::vector<Token> BitLexer::Tokenize() {
    std::vector<Token> tokens;
    while (pos < src.size()) {
        char c = src[pos];
        if (isspace(c)) {
            if (c == '\n') line++;
            pos++;
        }
        else if (c == '#') { // Comment
            while (pos < src.size() && src[pos] != '\n') pos++;
        }
        else if (isdigit(c)) {
            std::string s;
            while (pos < src.size() && (isdigit(src[pos]) || src[pos] == '.')) s += src[pos++];
            if (pos < src.size() && isalpha(src[pos])) {
                 while (pos < src.size() && isalnum(src[pos])) s += src[pos++];
            }
            tokens.push_back({TokenType::Number, s, line});
        }
        else if (isalpha(c) || c == '_') {
            std::string s;
            while (pos < src.size() && (isalnum(src[pos]) || src[pos] == '_')) s += src[pos++];
            if (IsKeyword(s)) tokens.push_back({TokenType::Keyword, s, line});
            else tokens.push_back({TokenType::Identifier, s, line});
        }
        else if (c == '"') {
            pos++;
            std::string s;
            while (pos < src.size() && src[pos] != '"') {
                if (src[pos] == '\\' && pos + 1 < src.size()) { pos++; s += src[pos++]; }
                else s += src[pos++];
            }
            if (pos < src.size()) pos++;
            tokens.push_back({TokenType::String, s, line});
        }
        else if (c == '-' && pos + 1 < src.size() && src[pos+1] == '>') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "->", line});
        }
        else if (c == '+' && pos + 1 < src.size() && src[pos+1] == '+') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "++", line});
        }
        else if (c == '-' && pos + 1 < src.size() && src[pos+1] == '-') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "--", line});
        }
        else if (c == '+' && pos + 1 < src.size() && src[pos+1] == '=') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "+=", line});
        }
        else if (c == '-' && pos + 1 < src.size() && src[pos+1] == '=') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "-=", line});
        }
        else if (c == '*' && pos + 1 < src.size() && src[pos+1] == '=') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "*=", line});
        }
        else if (c == '/' && pos + 1 < src.size() && src[pos+1] == '=') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "/=", line});
        }
        else if (c == '=' && pos + 1 < src.size() && src[pos+1] == '=') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "==", line});
        }
        else if (c == '!' && pos + 1 < src.size() && src[pos+1] == '=') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "!=", line});
        }
        else if (c == '>' && pos + 1 < src.size() && src[pos+1] == '=') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, ">=", line});
        }
        else if (c == '<' && pos + 1 < src.size() && src[pos+1] == '=') {
            pos += 2;
            tokens.push_back({TokenType::Symbol, "<=", line});
        }
        else {
            std::string s(1, c);
            tokens.push_back({TokenType::Symbol, s, line});
            pos++;
        }
    }
    tokens.push_back({TokenType::EndOfFile, "", line});
    return tokens;
}

bool BitLexer::IsKeyword(const std::string& s) {
    static const std::vector<std::string> keywords = {
        "var", "scene", "sprite", "choice", "jump", "if", "and", "or", "true", "false",
        "bg", "bgm", "sfx", "ui", "halt", "return", "call", "local", "wait", "shake",
        "delay", "expression", "hide", "pos", "clear", "random", "fade", "move",
        "fade_screen", "narration", "alias", "timeline", "play", "leave",
        "event", "emit", "parallel", "await", "play_sequence"
    };
    return std::find(keywords.begin(), keywords.end(), s) != keywords.end();
}
