#include "BitParser.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Syntax Analysis & Bytecode Generation: Tokens → Bytecode
// ─────────────────────────────────────────────────────────────────────────────
// Parses BitScript according to grammar rules and emits bytecode instructions.
// ─────────────────────────────────────────────────────────────────────────────

BitParser::BitParser(const std::vector<Token>& tokens, BitProject& p) 
    : tokens(tokens), p(p), pos(0), tempVarCount(0), currentScene("") {
    m_currentOutput = &p.bytecode;
}

Token BitParser::peek() { return tokens[pos]; }
Token BitParser::peekNext() { if (pos+1 < tokens.size()) return tokens[pos+1]; return tokens.back(); }
Token BitParser::consume() { return tokens[pos++]; }
Token BitParser::previous() { return tokens[pos-1]; }

bool BitParser::match(TokenType t, const std::string& v) {
    if (peek().type == t && (v == "" || peek().value == v)) {
        consume();
        return true;
    }
    return false;
}

void BitParser::expect(TokenType t, const std::string& v) {
    if (!match(t, v)) {
        std::string err = "Parse error line " + std::to_string(peek().line) + 
                          ": Expected '" + v + "' but found '" + peek().value + "'";
        p.parseErrors.push_back(err);
    }
}

std::string BitParser::genTempVar() {
    return "__tmp" + std::to_string(tempVarCount++);
}

void BitParser::emit(BitOp op, std::vector<std::string> args, nlohmann::json meta) {
    int line = 0;
    if (pos > 0 && pos <= tokens.size()) line = tokens[pos-1].line;
    else if (!tokens.empty()) line = tokens[0].line;
    m_currentOutput->push_back({op, args, meta, line});
}

void BitParser::Parse() {
    while (peek().type != TokenType::EndOfFile) {
        if (match(TokenType::Keyword, "config") || match(TokenType::Keyword, "assets") || match(TokenType::Keyword, "entities")) {
            std::string err = "Parse error line " + std::to_string(previous().line) + 
                              ": Legacy configuration blocks ('config', 'assets', 'entities') are no longer supported. Please use project.json and the entities/ folder.";
            p.parseErrors.push_back(err);
            // Try to skip block
            if (match(TokenType::Symbol, "{")) {
                int depth = 1;
                while (peek().type != TokenType::EndOfFile && depth > 0) {
                    if (peek().value == "{") depth++;
                    else if (peek().value == "}") depth--;
                    consume();
                }
            }
        }
        else if (match(TokenType::Keyword, "var")) ParseVariable();
        else if (match(TokenType::Keyword, "scene")) ParseScene();
        else if (match(TokenType::Keyword, "timeline")) ParseTimeline();
        else if (match(TokenType::Keyword, "event")) ParseEvent();
        else consume(); // skip unknown
    }
    emit(BitOp::HALT);
}

void BitParser::ParseEvent() {
    std::string evtName = consume().value;
    expect(TokenType::Symbol, "{");
    
    auto* prevOutput = m_currentOutput;
    m_currentOutput = &p.events[evtName];
    
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        ParseStatement();
    }
    expect(TokenType::Symbol, "}");
    
    m_currentOutput = prevOutput;
}

void BitParser::ParseVariable() {
    std::string id = consume().value;
    int initial = 0;
    if (match(TokenType::Symbol, "=")) {
        initial = std::stoi(consume().value);
    }
    p.variables[id] = {id, initial};
    expect(TokenType::Symbol, ";");
}

void BitParser::ParseScene() {
    std::string name = consume().value;
    std::vector<std::string> params = { name };
    if (match(TokenType::Symbol, "(")) {
        while (peek().type != TokenType::EndOfFile && peek().value != ")") {
            params.push_back(consume().value);
            match(TokenType::Symbol, ",");
        }
        expect(TokenType::Symbol, ")");
    }
    emit(BitOp::LABEL, params);
    expect(TokenType::Symbol, "{");
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        ParseStatement();
    }
    expect(TokenType::Symbol, "}");
    if(!m_currentOutput->empty() && m_currentOutput->back().op != BitOp::RETURN) {
        emit(BitOp::RETURN);
    }
    match(TokenType::Symbol, ";");
}

std::string BitParser::ParseTimeline() {
    std::string id = "";
    if (peek().type == TokenType::Identifier) {
        id = consume().value;
    } else {
        id = "anon_tl_" + std::to_string(tempVarCount++);
    }

    Timeline tl; tl.id = id;
    expect(TokenType::Symbol, "{");
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        std::string timeStr = consume().value;
        int time_ms = ParseTime(timeStr);
        expect(TokenType::Symbol, ":");
        
        std::vector<BitInstruction> temp;
        std::vector<BitInstruction>* old = m_currentOutput;
        m_currentOutput = &temp;
        
        if (match(TokenType::Symbol, "{")) {
            while (peek().type != TokenType::EndOfFile && peek().value != "}") {
                ParseStatement();
            }
            expect(TokenType::Symbol, "}");
        } else {
            ParseStatement();
        }
        
        m_currentOutput = old;
        
        for (auto& ins : temp) {
            tl.events.push_back({time_ms, ins.op, ins.args, ins.metadata});
        }
    }
    expect(TokenType::Symbol, "}");
    match(TokenType::Symbol, ";");
    p.timelines[id] = tl;
    return id;
}

int BitParser::ParseTime(const std::string& s) {
    if (s.empty()) return 0;
    try {
        if (s.size() > 2 && s.substr(s.size() - 2) == "ms") {
            return (int)std::stof(s.substr(0, s.size() - 2));
        }
        if (s.size() > 1 && s.back() == 's') {
            return (int)(std::stof(s.substr(0, s.size() - 1)) * 1000.0f);
        }
        return (int)std::stof(s); // Default to ms
    } catch (...) {
        return 0;
    }
}

