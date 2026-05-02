#include "headers/BitScriptInterpreter.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

BitScriptLexer::BitScriptLexer(const std::string& src) : src(src), pos(0), line(1) {}

std::vector<Token> BitScriptLexer::Tokenize() {
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

bool BitScriptLexer::IsKeyword(const std::string& s) {
    static const std::vector<std::string> keywords = {
        "config", "var", "entities", "assets", "scene", "sprite", "choice", "jump", "if", "and", "or", "true", "false", 
        "bg", "bgm", "ui", "halt", "return", "call", "local", "wait", "shake", "delay", "play_sfx", 
        "expression", "hide", "pos", "clear", "random", "fade", "move", "fade_screen", "narration", "alias",
        "timeline", "play", "leave"
    };
    return std::find(keywords.begin(), keywords.end(), s) != keywords.end();
}

BitScriptParser::BitScriptParser(const std::vector<Token>& tokens, DialogProject& p) 
    : tokens(tokens), p(p), pos(0), tempVarCount(0), currentScene("") {
    m_currentOutput = &p.bytecode;
}

Token BitScriptParser::peek() { return tokens[pos]; }
Token BitScriptParser::peekNext() { if (pos+1 < tokens.size()) return tokens[pos+1]; return tokens.back(); }
Token BitScriptParser::consume() { return tokens[pos++]; }
Token BitScriptParser::previous() { return tokens[pos-1]; }

bool BitScriptParser::match(TokenType t, const std::string& v) {
    if (peek().type == t && (v == "" || peek().value == v)) {
        consume();
        return true;
    }
    return false;
}

void BitScriptParser::expect(TokenType t, const std::string& v) {
    if (!match(t, v)) {
        std::cerr << "[BitScript] Parse error line " << peek().line 
                  << ": Expected '" << v << "' but found '" << peek().value << "'\n";
    }
}

std::string BitScriptParser::genTempVar() {
    return "__tmp" + std::to_string(tempVarCount++);
}

void BitScriptParser::emit(BitOp op, std::vector<std::string> args, nlohmann::json meta) {
    int line = 0;
    if (pos > 0 && pos <= tokens.size()) line = tokens[pos-1].line;
    else if (!tokens.empty()) line = tokens[0].line;
    m_currentOutput->push_back({op, args, meta, line});
}

void BitScriptParser::Parse() {
    while (peek().type != TokenType::EndOfFile) {
        if (match(TokenType::Keyword, "config")) ParseConfig();
        else if (match(TokenType::Keyword, "var")) ParseVariable();
        else if (match(TokenType::Keyword, "entities")) ParseEntities();
        else if (match(TokenType::Keyword, "assets")) ParseAssets();
        else if (match(TokenType::Keyword, "scene")) ParseScene();
        else if (match(TokenType::Keyword, "timeline")) ParseTimeline();
        else consume(); // skip unknown
    }
    emit(BitOp::HALT);
}

void BitScriptParser::ParseConfig() {
    expect(TokenType::Symbol, "{");
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        std::string key = consume().value;
        expect(TokenType::Symbol, "=");
        std::string val = consume().value;
        if (key == "start_node") p.configs.start_node = val;
        else if (key == "mode") p.configs.mode = val;
        else if (key == "reveal_speed") p.configs.reveal_speed = std::stof(val);
        else if (key == "auto_save") p.configs.auto_save = (val == "true");
        match(TokenType::Symbol, ";");
    }
    expect(TokenType::Symbol, "}");
    match(TokenType::Symbol, ";");
}

void BitScriptParser::ParseVariable() {
    std::string id = consume().value;
    int initial = 0;
    if (match(TokenType::Symbol, "=")) {
        initial = std::stoi(consume().value);
    }
    p.variables[id] = {id, initial};
    expect(TokenType::Symbol, ";");
}

void BitScriptParser::ParseEntities() {
    expect(TokenType::Symbol, "{");
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        std::string id = consume().value;
        Entity e; e.id = id;
        expect(TokenType::Symbol, "{");
        while (peek().type != TokenType::EndOfFile && peek().value != "}") {
            std::string key = consume().value;
            if (key == "sprite") {
                std::string sname = consume().value;
                expect(TokenType::Symbol, "{");
                SpriteDef sd;
                while (peek().type != TokenType::EndOfFile && peek().value != "}") {
                    std::string sk = consume().value;
                    expect(TokenType::Symbol, "=");
                    if (sk == "path") sd.path = consume().value;
                    else if (sk == "frames") sd.frames = std::stoi(consume().value);
                    else if (sk == "speed") sd.speed = std::stof(consume().value);
                    else if (sk == "scale") sd.scale = std::stof(consume().value);
                    match(TokenType::Symbol, ";");
                }
                expect(TokenType::Symbol, "}");
                e.sprites[sname] = sd;
            }
            else if (key == "alias") {
                std::string aname = consume().value;
                expect(TokenType::Symbol, "{");
                nlohmann::json aj;
                while (peek().type != TokenType::EndOfFile && peek().value != "}") {
                    std::string ak = consume().value;
                    expect(TokenType::Symbol, "=");
                    aj[ak] = consume().value;
                    match(TokenType::Symbol, ",");
                    match(TokenType::Symbol, ";");
                }
                expect(TokenType::Symbol, "}");
                e.aliases[aname] = aj;
            }
            else {
                expect(TokenType::Symbol, "=");
                std::string val = consume().value;
                if (key == "name") e.name = val;
                else if (key == "type") e.type = val;
                else if (key == "pos_x" || key == "default_pos_x" || key == "default_pos") {
                    if (val == "left") e.default_pos_x = 0.2f;
                    else if (val == "right") e.default_pos_x = 0.8f;
                    else if (val == "center") e.default_pos_x = 0.5f;
                    else e.default_pos_x = std::stof(val);
                }
            }
            match(TokenType::Symbol, ";");
        }
        expect(TokenType::Symbol, "}");
        match(TokenType::Symbol, ";");
        p.entities[id] = e;
    }
    expect(TokenType::Symbol, "}");
    match(TokenType::Symbol, ";");
}

void BitScriptParser::ParseAssets() {
    expect(TokenType::Symbol, "{");
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        std::string type = consume().value;
        expect(TokenType::Symbol, "{");
        while (peek().type != TokenType::EndOfFile && peek().value != "}") {
            std::string id = consume().value;
            expect(TokenType::Symbol, "=");
            std::string path = consume().value;
            if (type == "bg" || type == "backgrounds") p.backgrounds[id] = path;
            else if (type == "music") p.music[id] = path;
            else if (type == "sfx") p.sfx[id] = path;
            else if (type == "fonts") p.fonts[id] = path;
            match(TokenType::Symbol, ";");
        }
        expect(TokenType::Symbol, "}");
        match(TokenType::Symbol, ";");
    }
    expect(TokenType::Symbol, "}");
    match(TokenType::Symbol, ";");
}

void BitScriptParser::ParseScene() {
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
    match(TokenType::Symbol, ";");
}

std::string BitScriptParser::ParseTimeline() {
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
        int time_ms = 0;
        if (timeStr.size() > 2 && timeStr.substr(timeStr.size() - 2) == "ms") {
            time_ms = std::stoi(timeStr.substr(0, timeStr.size() - 2));
        } else {
            try { time_ms = std::stoi(timeStr); } catch(...) { time_ms = 0; }
        }
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

void BitScriptParser::ParseStatement() {
    if (match(TokenType::Keyword, "scene")) ParseScene();
    else if (match(TokenType::Keyword, "timeline")) ParseTimeline();
    else if (match(TokenType::Keyword, "leave")) {
        std::string entityId = consume().value;
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "leave"; j["target"] = entityId;
        emit(BitOp::EVENT, {"leave"}, j);
    }
    else if (match(TokenType::Keyword, "ui")) {
        std::string action = consume().value;
        expect(TokenType::Symbol, ";");
        emit(BitOp::UI_VISIBLE, {action});
    }
    else if (match(TokenType::Keyword, "narration")) {
        nlohmann::json meta;
        if (match(TokenType::Symbol, "[")) {
            while (peek().type != TokenType::EndOfFile && peek().value != "]") {
                std::string k = consume().value;
                expect(TokenType::Symbol, "=");
                std::string v = consume().value;
                meta[k] = v;
                match(TokenType::Symbol, ",");
            }
            expect(TokenType::Symbol, "]");
        }
        match(TokenType::Symbol, ":");
        std::string text = consume().value;
        expect(TokenType::Symbol, ";");
        emit(BitOp::SAY, {"narration", text}, meta);
    }
    else if (peek().type == TokenType::Identifier && (peekNext().value == ":" || peekNext().value == "[" || peekNext().value == "." || peekNext().value == "=" || peekNext().value == "{")) {
        std::string entity = consume().value;
        if (match(TokenType::Symbol, "=")) {
            ParseAssignment(entity, *m_currentOutput);
            return;
        }
        
        if (match(TokenType::Symbol, "{")) {
            ParseDialogueBlock(entity, *m_currentOutput);
            return;
        }

        std::string alias = "";
        if (match(TokenType::Symbol, ".")) {
            alias = consume().value;
        }
        
        nlohmann::json meta;
        if (!alias.empty()) meta["alias"] = alias;

        if (match(TokenType::Symbol, "[")) {
            while (peek().type != TokenType::EndOfFile && peek().value != "]") {
                std::string k = consume().value;
                expect(TokenType::Symbol, "=");
                std::string v = consume().value;
                meta[k] = v;
                match(TokenType::Symbol, ",");
            }
            expect(TokenType::Symbol, "]");
        }
        
        std::string text = "";
        if (match(TokenType::Symbol, ":")) {
            text = consume().value;
        }
        expect(TokenType::Symbol, ";");
        emit(BitOp::SAY, {entity, text}, meta);
    }
    else if (match(TokenType::Keyword, "choice")) {
        expect(TokenType::Symbol, "{");
        while (peek().type != TokenType::EndOfFile && peek().value != "}") {
            std::string text = consume().value;
            nlohmann::json optMeta;
            if (match(TokenType::Symbol, "[")) {
                while (peek().type != TokenType::EndOfFile && peek().value != "]") {
                    std::string k = consume().value;
                    expect(TokenType::Symbol, "=");
                    std::string v = consume().value;
                    optMeta[k] = v;
                    match(TokenType::Symbol, ",");
                }
                expect(TokenType::Symbol, "]");
            }
            expect(TokenType::Symbol, "->");
            std::string target = consume().value;
            
            if (match(TokenType::Keyword, "if")) {
                std::string labelChoice = genTempVar() + "_choice";
                std::string labelSkip = genTempVar() + "_skip";
                
                expect(TokenType::Symbol, "(");
                std::string var = consume().value;
                std::string op = consume().value;
                Operand val = ParseExpression(*m_currentOutput);
                expect(TokenType::Symbol, ")");
                
                if (val.isRef) emit(BitOp::IF_REF, {var, op, val.val, labelChoice});
                else emit(BitOp::IF, {var, op, val.val, labelChoice});
                
                emit(BitOp::GOTO, {labelSkip});
                emit(BitOp::LABEL, {labelChoice});
                emit(BitOp::CHOICE, {text, target}, optMeta);
                emit(BitOp::LABEL, {labelSkip});
            } else {
                emit(BitOp::CHOICE, {text, target}, optMeta);
            }
            match(TokenType::Symbol, ";");
        }
        expect(TokenType::Symbol, "}");
        match(TokenType::Symbol, ";");
        emit(BitOp::WAIT_INPUT);
    }
    else if (match(TokenType::Keyword, "halt")) {
        expect(TokenType::Symbol, ";");
        emit(BitOp::HALT);
    }
    else if (match(TokenType::Keyword, "local")) {
        std::string var = consume().value;
        expect(TokenType::Symbol, "=");
        ParseAssignment(var, *m_currentOutput, true);
    }
    else if (match(TokenType::Keyword, "shake")) {
        Operand intensity = ParseExpression(*m_currentOutput);
        bool wait = match(TokenType::Keyword, "wait");
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "shake"; j["intensity"] = std::stof(intensity.val);
        emit(BitOp::EVENT, {"shake"}, j);
        if (wait) emit(BitOp::WAIT_ACTION, {"all"}); // Shake usually implies all
    }
    else if (match(TokenType::Keyword, "delay")) {
        Operand dur = ParseExpression(*m_currentOutput);
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "delay"; j["duration"] = std::stoi(dur.val);
        emit(BitOp::EVENT, {"delay"}, j);
    }
    else if (match(TokenType::Keyword, "play_sfx")) {
        std::string id = consume().value;
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "play_sfx"; j["id"] = id;
        emit(BitOp::EVENT, {"play_sfx"}, j);
    }
    else if (match(TokenType::Keyword, "expression")) {
        std::string target = consume().value;
        expect(TokenType::Symbol, ",");
        std::string id = consume().value;
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "expression"; j["target"] = target; j["id"] = id;
        emit(BitOp::EVENT, {"expression"}, j);
    }
    else if (match(TokenType::Keyword, "hide")) {
        std::string target = consume().value;
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "hide"; j["target"] = target;
        emit(BitOp::EVENT, {"hide"}, j);
    }
    else if (match(TokenType::Keyword, "pos")) {
        std::string target = consume().value;
        expect(TokenType::Symbol, ",");
        std::string x = consume().value;
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "pos"; j["target"] = target; j["x"] = x;
        emit(BitOp::EVENT, {"pos"}, j);
    }
    else if (match(TokenType::Keyword, "clear")) {
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "clear";
        emit(BitOp::EVENT, {"clear"}, j);
    }
    else if (match(TokenType::Keyword, "random")) {
        std::string var = consume().value;
        expect(TokenType::Symbol, ",");
        Operand lo = ParseExpression(*m_currentOutput);
        expect(TokenType::Symbol, ",");
        Operand hi = ParseExpression(*m_currentOutput);
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "random"; j["var"] = var;
        j["min"] = std::stoi(lo.val); j["max"] = std::stoi(hi.val);
        emit(BitOp::EVENT, {"random"}, j);
    }
    else if (match(TokenType::Keyword, "fade")) {
        std::string target = consume().value;
        expect(TokenType::Symbol, ",");
        std::string valOrId = consume().value;
        expect(TokenType::Symbol, ",");
        std::string dur = consume().value;
        
        bool wait = match(TokenType::Keyword, "wait");
        expect(TokenType::Symbol, ";");
        
        nlohmann::json j; j["op"] = "fade"; j["target"] = target;
        if (target == "bg") j["id"] = valOrId;
        else j["alpha"] = std::stof(valOrId);
        j["duration"] = std::stoi(dur);
        emit(BitOp::EVENT, {"fade"}, j);
        if (wait) emit(BitOp::WAIT_ACTION, {"fade"});
    }
    else if (match(TokenType::Keyword, "move")) {
        std::string target = consume().value;
        expect(TokenType::Symbol, ",");
        std::string x = consume().value;
        expect(TokenType::Symbol, ",");
        std::string dur = consume().value;
        
        bool wait = match(TokenType::Keyword, "wait");
        expect(TokenType::Symbol, ";");
        
        nlohmann::json j; j["op"] = "move"; j["target"] = target;
        j["x"] = x; j["duration"] = std::stoi(dur);
        emit(BitOp::EVENT, {"move"}, j);
        if (wait) emit(BitOp::WAIT_ACTION, {"move"});
    }
    else if (match(TokenType::Keyword, "fade_screen")) {
        std::string alpha = consume().value;
        expect(TokenType::Symbol, ",");
        std::string dur = consume().value;
        
        bool wait = match(TokenType::Keyword, "wait");
        expect(TokenType::Symbol, ";");
        
        nlohmann::json j; j["op"] = "fade_screen"; j["alpha"] = std::stof(alpha); j["duration"] = std::stoi(dur);
        emit(BitOp::EVENT, {"fade_screen"}, j);
        if (wait) emit(BitOp::WAIT_ACTION, {"fade"});
    }
    else if (match(TokenType::Keyword, "bg")) {
        expect(TokenType::Symbol, "=");
        std::string val = consume().value;
        expect(TokenType::Symbol, ";");
        emit(BitOp::BG, {val});
    }
    else if (match(TokenType::Keyword, "bgm")) {
        expect(TokenType::Symbol, "=");
        std::string val = consume().value;
        expect(TokenType::Symbol, ";");
        emit(BitOp::BGM, {val});
    }
    else if (match(TokenType::Keyword, "jump")) {
        std::string target = consume().value;
        expect(TokenType::Symbol, ";");
        emit(BitOp::GOTO, {target});
    }
    else if (match(TokenType::Keyword, "call")) {
        std::string label = consume().value;
        std::vector<std::string> args = { label };
        if (match(TokenType::Symbol, "(")) {
            while (peek().type != TokenType::EndOfFile && peek().value != ")") {
                Operand arg = ParseExpression(*m_currentOutput);
                args.push_back(arg.val);
                match(TokenType::Symbol, ",");
            }
            expect(TokenType::Symbol, ")");
        }
        expect(TokenType::Symbol, ";");
        emit(BitOp::CALL, args);
    }
    else if (match(TokenType::Keyword, "return")) {
        expect(TokenType::Symbol, ";");
        emit(BitOp::RETURN);
    }
    else if (match(TokenType::Keyword, "play")) {
        expect(TokenType::Keyword, "timeline");
        std::string tid;
        if (peek().value == "{") {
            tid = ParseTimeline();
        } else {
            tid = consume().value;
        }
        std::string wait = "false";
        if (match(TokenType::Keyword, "wait")) wait = "true";
        expect(TokenType::Symbol, ";");
        emit(BitOp::PLAY_TIMELINE, {tid, wait});
    }
    else if (match(TokenType::Keyword, "if")) {
        ParseIfStatement(*m_currentOutput);
    }
    else {
        consume(); // skip unknown
    }
}

void BitScriptParser::ParseAssignment(const std::string& var, std::vector<BitInstruction>& output, bool isLocal) {
    if (peek().type == TokenType::Identifier && peek().value == var && 
        (peekNext().value == "+" || peekNext().value == "-" || peekNext().value == "*" || peekNext().value == "/")) {
        consume(); // var
        std::string op = consume().value;
        Operand right = ParseExpression(output);
        expect(TokenType::Symbol, ";");

        BitOp bop;
        if (op == "+") bop = right.isRef ? BitOp::ADD_REF : BitOp::ADD;
        else if (op == "-") bop = right.isRef ? BitOp::SUB_REF : BitOp::SUB;
        else if (op == "*") bop = right.isRef ? BitOp::MUL_REF : BitOp::MUL;
        else bop = right.isRef ? BitOp::DIV_REF : BitOp::DIV;

        emit(bop, {var, right.val});
        return;
    }

    Operand res = ParseExpression(output);
    expect(TokenType::Symbol, ";");
    
    if (isLocal) {
        emit(BitOp::SET_LOCAL, {var, res.val});
    } else {
        if (res.isRef) emit(BitOp::SET_REF, {var, res.val});
        else emit(BitOp::SET, {var, res.val});
    }
}

void BitScriptParser::ParseDialogueBlock(const std::string& entityId, std::vector<BitInstruction>& output) {
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        std::string alias = "";
        if (match(TokenType::Symbol, ".")) {
            alias = consume().value;
        }
        
        nlohmann::json meta;
        if (!alias.empty()) meta["alias"] = alias;

        if (match(TokenType::Symbol, "[")) {
            while (peek().type != TokenType::EndOfFile && peek().value != "]") {
                std::string k = consume().value;
                expect(TokenType::Symbol, "=");
                std::string v = consume().value;
                meta[k] = v;
                match(TokenType::Symbol, ",");
            }
            expect(TokenType::Symbol, "]");
        }
        
        match(TokenType::Symbol, ":");
        
        if (peek().type == TokenType::String) {
            std::string text = consume().value;
            expect(TokenType::Symbol, ";");
            emit(BitOp::SAY, {entityId, text}, meta);
        } else {
            consume();
        }
    }
    expect(TokenType::Symbol, "}");
    match(TokenType::Symbol, ";");
}

Operand BitScriptParser::ParseExpression(std::vector<BitInstruction>& output) {
    return ParseAddExpr(output);
}

Operand BitScriptParser::ParseAddExpr(std::vector<BitInstruction>& output) {
    Operand left = ParseMulExpr(output);
    while (peek().type != TokenType::EndOfFile && (peek().value == "+" || peek().value == "-")) {
        std::string op = consume().value;
        Operand right = ParseMulExpr(output);
        
        // For literal operations, compute directly
        if (!left.isRef && !right.isRef) {
            try {
                int lval = std::stoi(left.val);
                int rval = std::stoi(right.val);
                int result = (op == "+") ? (lval + rval) : (lval - rval);
                left.val = std::to_string(result);
            } catch (...) {
                // Conversion failed, keep left
            }
        } else {
            // For variable operations, emit instructions
            std::string resultVar = genTempVar();
            if (op == "+") {
                if (left.isRef && right.isRef) {
                    emit(BitOp::SET_REF, {resultVar, left.val});
                    emit(BitOp::ADD_REF, {resultVar, right.val});
                } else if (left.isRef) {
                    emit(BitOp::SET_REF, {resultVar, left.val});
                    emit(BitOp::ADD, {resultVar, right.val});
                } else {
                    emit(BitOp::SET, {resultVar, left.val});
                    emit(BitOp::ADD_REF, {resultVar, right.val});
                }
            } else { // op == "-"
                if (left.isRef && right.isRef) {
                    emit(BitOp::SET_REF, {resultVar, left.val});
                    emit(BitOp::SUB_REF, {resultVar, right.val});
                } else if (left.isRef) {
                    emit(BitOp::SET_REF, {resultVar, left.val});
                    emit(BitOp::SUB, {resultVar, right.val});
                } else {
                    emit(BitOp::SET, {resultVar, left.val});
                    emit(BitOp::SUB_REF, {resultVar, right.val});
                }
            }
            left = {false, resultVar};
        }
    }
    return left;
}

Operand BitScriptParser::ParseMulExpr(std::vector<BitInstruction>& output) {
    Operand left = ParsePrimary();
    while (peek().type != TokenType::EndOfFile && (peek().value == "*" || peek().value == "/")) {
        std::string op = consume().value;
        Operand right = ParsePrimary();
        
        // For literal operations, compute directly
        if (!left.isRef && !right.isRef) {
            try {
                int lval = std::stoi(left.val);
                int rval = std::stoi(right.val);
                if (op == "/" && rval == 0) {
                    left.val = "0"; // Division by zero at parse time - default to 0
                } else {
                    int result = (op == "*") ? (lval * rval) : (lval / rval);
                    left.val = std::to_string(result);
                }
            } catch (...) {
                left.val = "0";
            }
        } else {
            // For variable operations, emit instructions
            std::string resultVar = genTempVar();
            if (op == "*") {
                if (left.isRef && right.isRef) {
                    emit(BitOp::SET_REF, {resultVar, left.val});
                    emit(BitOp::MUL_REF, {resultVar, right.val});
                } else if (left.isRef) {
                    emit(BitOp::SET_REF, {resultVar, left.val});
                    emit(BitOp::MUL, {resultVar, right.val});
                } else {
                    emit(BitOp::SET, {resultVar, left.val});
                    emit(BitOp::MUL_REF, {resultVar, right.val});
                }
            } else { // op == "/"
                if (left.isRef && right.isRef) {
                    emit(BitOp::SET_REF, {resultVar, left.val});
                    emit(BitOp::DIV_REF, {resultVar, right.val});
                } else if (left.isRef) {
                    emit(BitOp::SET_REF, {resultVar, left.val});
                    emit(BitOp::DIV, {resultVar, right.val});
                } else {
                    emit(BitOp::SET, {resultVar, left.val});
                    emit(BitOp::DIV_REF, {resultVar, right.val});
                }
            }
            left = {false, resultVar};
        }
    }
    return left;
}

Operand BitScriptParser::ParsePrimary() {
    Token t = consume();
    if (t.type == TokenType::Number) return {false, t.value};
    if (t.type == TokenType::Identifier) return {true, t.value};
    if (t.type == TokenType::String) return {false, t.value};
    return {false, "0"};
}

void BitScriptParser::ParseIfStatement(std::vector<BitInstruction>& output) {
    expect(TokenType::Symbol, "(");
    
    struct Cond { std::string var, op, val; bool isRef; };
    std::vector<Cond> conditions;

    auto parseOne = [&]() {
        std::string var = consume().value;
        std::string op = consume().value;
        Operand val = ParseExpression(output);
        conditions.push_back({var, op, val.val, val.isRef});
    };

    parseOne();
    while (peek().type != TokenType::EndOfFile && (peek().value == "and" || peek().value == "or")) {
        consume(); // consume "and" or "or" - for now only AND logic is properly supported
        parseOne();
    }
    
    expect(TokenType::Symbol, ")");
    expect(TokenType::Symbol, "{");
    
    std::string endLabel = genTempVar() + "_end";

    // Helper to negate operator
    auto negateOp = [](const std::string& op) -> std::string {
        if (op == "==" || op == "=") return "!=";
        if (op == "!=") return "==";
        if (op == ">") return "<=";
        if (op == "<") return ">=";
        if (op == ">=") return "<";
        if (op == "<=") return ">";
        return op;
    };

    // Emit AND logic: jump to end if ANY condition is false
    for (size_t i = 0; i < conditions.size(); ++i) {
        auto& c = conditions[i];
        std::string negate = negateOp(c.op);
        if (c.isRef) emit(BitOp::IF_REF, {c.var, negate, c.val, endLabel});
        else emit(BitOp::IF, {c.var, negate, c.val, endLabel});
    }
    
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        ParseStatement();
    }
    expect(TokenType::Symbol, "}");
    match(TokenType::Symbol, ";");
    
    emit(BitOp::LABEL, {endLabel});
}

bool BitScriptInterpreter::LoadScriptFile(const std::string& path, DialogProject& p) {
    std::ifstream f(path);
    if (!f) return false;
    std::stringstream buffer;
    buffer << f.rdbuf();
    
    BitScriptLexer lexer(buffer.str());
    auto tokens = lexer.Tokenize();
    
    BitScriptParser parser(tokens, p);
    parser.Parse();
    
    return true;
}
