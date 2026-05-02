#include "headers/BitScriptInterpreter.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

// --- Lexer Implementation ---

BitScriptLexer::BitScriptLexer(const std::string& src) : src(src), pos(0), line(1) {}

bool BitScriptLexer::IsKeyword(const std::string& s) {
    static const std::vector<std::string> kw = {
        "config", "var", "entities", "assets", "backgrounds", "music", "sfx", "fonts",
        "sprite", "scene", "choice", "jump", "if", "and", "or", "true", "false", 
        "left", "right", "center", "bg", "bgm",
        "shake", "delay", "fade", "move", "fade_screen", "halt",
        "play_sfx", "expression", "hide", "pos", "clear", "random",
        "transition", "ui", "narration", "call", "return", "local", "wait", "alias"
    };
    for (const auto& k : kw) if (k == s) return true;
    return false;
}

std::vector<Token> BitScriptLexer::Tokenize() {
    std::vector<Token> tokens;
    while (pos < src.length()) {
        char c = src[pos];
        if (isspace(c)) {
            if (c == '\n') line++;
            pos++;
            continue;
        }
        if (c == '#') {
            if (pos + 1 < src.length() && src[pos+1] == '-') {
                pos += 2; // skip #-
                while (pos + 1 < src.length() && !(src[pos] == '-' && src[pos+1] == '#')) {
                    if (src[pos] == '\n') line++;
                    pos++;
                }
                if (pos + 1 < src.length()) pos += 2; // skip -#
                continue;
            } else {
                while (pos < src.length() && src[pos] != '\n') pos++;
                continue;
            }
        }
        if (isalpha(c) || c == '_') {
            std::string ident;
            while (pos < src.length() && (isalnum(src[pos]) || src[pos] == '_')) {
                ident += src[pos++];
            }
            if (IsKeyword(ident)) tokens.push_back({TokenType::Keyword, ident, line});
            else tokens.push_back({TokenType::Identifier, ident, line});
            continue;
        }
        if (isdigit(c) || (c == '-' && pos + 1 < src.length() && isdigit(src[pos+1]))) {
            std::string num;
            if (c == '-') num += src[pos++];
            while (pos < src.length() && (isdigit(src[pos]) || src[pos] == '.')) {
                num += src[pos++];
            }
            tokens.push_back({TokenType::Number, num, line});
            continue;
        }
        if (c == '"') {
            pos++;
            std::string str;
            while (pos < src.length() && src[pos] != '"') {
                str += src[pos++];
            }
            if (pos < src.length()) pos++;
            tokens.push_back({TokenType::String, str, line});
            continue;
        }
        
        std::string sym(1, c);
        if (pos + 1 < src.length()) {
            std::string two = src.substr(pos, 2);
            if (two == "==" || two == "!=" || two == "<=" || two == ">=" || two == "->") {
                tokens.push_back({TokenType::Symbol, two, line});
                pos += 2;
                continue;
            }
        }
        tokens.push_back({TokenType::Symbol, sym, line});
        pos++;
    }
    tokens.push_back({TokenType::EndOfFile, "", line});
    return tokens;
}

// --- Parser Implementation ---

BitScriptParser::BitScriptParser(const std::vector<Token>& tokens, DialogProject& p) 
    : tokens(tokens), p(p), pos(0), tempVarCount(0) {}

Token BitScriptParser::peek() { return pos < tokens.size() ? tokens[pos] : tokens.back(); }
Token BitScriptParser::peekNext() { return pos + 1 < tokens.size() ? tokens[pos+1] : tokens.back(); }
Token BitScriptParser::consume() { return pos < tokens.size() ? tokens[pos++] : tokens.back(); }
Token BitScriptParser::previous() { return pos > 0 ? tokens[pos-1] : tokens[0]; }

bool BitScriptParser::match(TokenType t, const std::string& v) {
    if (peek().type == t && (v.empty() || peek().value == v)) {
        consume(); return true;
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

void BitScriptParser::Parse() {
    while (peek().type != TokenType::EndOfFile) {
        if (match(TokenType::Keyword, "config")) ParseConfig();
        else if (match(TokenType::Keyword, "var")) ParseVariable();
        else if (match(TokenType::Keyword, "entities")) ParseEntities();
        else if (match(TokenType::Keyword, "assets")) ParseAssets();
        else if (match(TokenType::Keyword, "scene")) ParseScene();
        else consume(); // skip unknown
    }
    p.bytecode.push_back({BitOp::HALT, {}, {}});
}

void BitScriptParser::ParseConfig() {
    expect(TokenType::Symbol, "{");
    while (!match(TokenType::Symbol, "}")) {
        std::string key = consume().value;
        expect(TokenType::Symbol, "=");
        std::string val = consume().value;
        expect(TokenType::Symbol, ";");
        if (key == "start_node") p.configs.start_node = val;
        else if (key == "mode") p.configs.mode = val;
    }
    expect(TokenType::Symbol, ";");
}

void BitScriptParser::ParseVariable() {
    std::string name = consume().value;
    expect(TokenType::Symbol, "=");
    std::string val = consume().value;
    expect(TokenType::Symbol, ";");
    
    VariableDef def;
    def.id = name;
    def.initial_value = (val == "true") ? 1 : (val == "false") ? 0 : std::stoi(val);
    p.variables[name] = def;
}

void BitScriptParser::ParseEntities() {
    expect(TokenType::Symbol, "{");
    while (!match(TokenType::Symbol, "}")) {
        std::string id = consume().value;
        Entity ent; ent.id = id;
        expect(TokenType::Symbol, "{");
        while (!match(TokenType::Symbol, "}")) {
            if (match(TokenType::Keyword, "sprite")) {
                std::string sName = consume().value;
                expect(TokenType::Symbol, "{");
                SpriteDef sd;
                while (!match(TokenType::Symbol, "}")) {
                    std::string k = consume().value;
                    expect(TokenType::Symbol, "=");
                    std::string v = consume().value;
                    expect(TokenType::Symbol, ";");
                    if (k == "path") sd.path = v;
                    else if (k == "frames") sd.frames = std::stoi(v);
                    else if (k == "speed") sd.speed = std::stof(v);
                    else if (k == "scale") sd.scale = std::stof(v);
                }
                ent.sprites[sName] = sd;
                expect(TokenType::Symbol, ";");
            }
            else if (match(TokenType::Keyword, "alias")) {
                std::string aliasName = consume().value;
                expect(TokenType::Symbol, "{");
                nlohmann::json aliasMeta;
                while (!match(TokenType::Symbol, "}")) {
                    std::string key = consume().value;
                    expect(TokenType::Symbol, "=");
                    std::string val = consume().value;
                    aliasMeta[key] = val;
                    match(TokenType::Symbol, ",");
                }
                ent.aliases[aliasName] = aliasMeta;
                expect(TokenType::Symbol, ";");
            }
            else {
                std::string k = consume().value;
                expect(TokenType::Symbol, "=");
                std::string v = consume().value;
                expect(TokenType::Symbol, ";");
                if (k == "name") ent.name = v;
                else if (k == "type") ent.type = v;
                else if (k == "default_pos") {
                    if (v == "left") ent.default_pos_x = 0.2f;
                    else if (v == "right") ent.default_pos_x = 0.8f;
                    else if (v == "center") ent.default_pos_x = 0.5f;
                    else try { ent.default_pos_x = std::stof(v); } catch(...) { ent.default_pos_x = 0.5f; }
                }
            }
        }
        p.entities[id] = ent;
        expect(TokenType::Symbol, ";");
    }
    expect(TokenType::Symbol, ";");
}

void BitScriptParser::ParseAssets() {
    expect(TokenType::Symbol, "{");
    while (!match(TokenType::Symbol, "}")) {
        std::string type = consume().value;
        expect(TokenType::Symbol, "{");
        while (!match(TokenType::Symbol, "}")) {
            std::string id = consume().value;
            expect(TokenType::Symbol, "=");
            std::string path = consume().value;
            expect(TokenType::Symbol, ";");
            if (type == "backgrounds") p.backgrounds[id] = path;
            else if (type == "music") p.music[id] = path;
            else if (type == "sfx") p.sfx[id] = path;
            else if (type == "fonts") p.fonts[id] = path;
        }
        expect(TokenType::Symbol, ";");
    }
    expect(TokenType::Symbol, ";");
}

void BitScriptParser::ParseScene() {
    std::string name = consume().value;
    std::vector<std::string> params = { name };
    if (match(TokenType::Symbol, "(")) {
        while (!match(TokenType::Symbol, ")")) {
            params.push_back(consume().value);
            match(TokenType::Symbol, ",");
        }
    }
    p.bytecode.push_back({BitOp::LABEL, params, {}});
    expect(TokenType::Symbol, "{");
    while (!match(TokenType::Symbol, "}")) {
        if (match(TokenType::Keyword, "scene")) ParseScene();
        else if (match(TokenType::Symbol, ">")) {
            // Join character: > akira [sprite=idle];
            std::string entityId = consume().value;
            nlohmann::json meta;
            if (match(TokenType::Symbol, "[")) {
                while (!match(TokenType::Symbol, "]")) {
                    std::string k = consume().value;
                    expect(TokenType::Symbol, "=");
                    std::string v = consume().value;
                    meta[k] = v;
                    match(TokenType::Symbol, ",");
                }
            }
            expect(TokenType::Symbol, ";");
            meta["join"] = "true";
            if (!meta.contains("alpha")) meta["alpha"] = "1.0";
            p.bytecode.push_back({BitOp::EVENT, {"join"}, meta});
            // We use EVENT with op="join" but we should probably just use SAY with empty text
            p.bytecode.push_back({BitOp::SAY, {entityId, ""}, meta});
        }
        else if (match(TokenType::Symbol, "<")) {
            // Leave character: < akira;
            std::string entityId = consume().value;
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "hide"; j["target"] = entityId;
            p.bytecode.push_back({BitOp::EVENT, {"hide"}, j});
        }
        else if (match(TokenType::Keyword, "transition")) {
            // transition type, duration, post_delay;
            std::string type = consume().value;
            expect(TokenType::Symbol, ",");
            std::string duration = consume().value;
            std::string delay = "0";
            if (match(TokenType::Symbol, ",")) delay = consume().value;
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::TRANSITION, {type, duration, delay}, {}});
        }
        else if (match(TokenType::Keyword, "ui")) {
            // ui hide; ui show;
            std::string action = consume().value;
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::UI_VISIBLE, {action}, {}});
        }
        else if (match(TokenType::Keyword, "narration")) {
            // narration [meta]: "text"; or narration "text";
            nlohmann::json meta;
            if (match(TokenType::Symbol, "[")) {
                while (!match(TokenType::Symbol, "]")) {
                    std::string k = consume().value;
                    expect(TokenType::Symbol, "=");
                    std::string v = consume().value;
                    meta[k] = v;
                    match(TokenType::Symbol, ",");
                }
            }
            if (match(TokenType::Symbol, ":")) {
                // optional colon
            }
            std::string text = consume().value;
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::SAY, {"narration", text}, meta});
        }
        else if (peek().type == TokenType::Identifier && (peekNext().value == ":" || peekNext().value == "[" || peekNext().value == ".")) {
            std::string entity = consume().value;
            std::string alias = "";
            if (match(TokenType::Symbol, ".")) {
                alias = consume().value;
            }
            
            nlohmann::json meta;
            if (!alias.empty()) meta["alias"] = alias;

            if (match(TokenType::Symbol, "[")) {
                while (!match(TokenType::Symbol, "]")) {
                    std::string k = consume().value;
                    expect(TokenType::Symbol, "=");
                    std::string v = consume().value;
                    meta[k] = v;
                    match(TokenType::Symbol, ",");
                }
            }
            
            std::string text = "";
            if (match(TokenType::Symbol, ":")) {
                text = consume().value;
            }
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::SAY, {entity, text}, meta});
        }
        else if (match(TokenType::Keyword, "choice")) {
            expect(TokenType::Symbol, "{");
            while (!match(TokenType::Symbol, "}")) {
                std::string text = consume().value;
                nlohmann::json optMeta;
                if (match(TokenType::Symbol, "[")) {
                    while (!match(TokenType::Symbol, "]")) {
                        std::string k = consume().value;
                        expect(TokenType::Symbol, "=");
                        std::string v = consume().value;
                        optMeta[k] = v;
                        match(TokenType::Symbol, ",");
                    }
                }
                expect(TokenType::Symbol, "->");
                std::string target = consume().value;
                
                if (match(TokenType::Keyword, "if")) {
                    // Emit: IF (cond) GOTO label_choice; GOTO label_skip; label_choice: CHOICE text target; label_skip:
                    std::string labelChoice = genTempVar() + "_choice";
                    std::string labelSkip = genTempVar() + "_skip";
                    
                    expect(TokenType::Symbol, "(");
                    std::string var = consume().value;
                    std::string op = consume().value;
                    Operand val = ParseExpression(p.bytecode);
                    expect(TokenType::Symbol, ")");
                    
                    if (val.isRef) p.bytecode.push_back({BitOp::IF_REF, {var, op, val.val, labelChoice}, {}});
                    else p.bytecode.push_back({BitOp::IF, {var, op, val.val, labelChoice}, {}});
                    
                    p.bytecode.push_back({BitOp::GOTO, {labelSkip}, {}});
                    p.bytecode.push_back({BitOp::LABEL, {labelChoice}, {}});
                    p.bytecode.push_back({BitOp::CHOICE, {text, target}, optMeta});
                    p.bytecode.push_back({BitOp::LABEL, {labelSkip}, {}});
                } else {
                    p.bytecode.push_back({BitOp::CHOICE, {text, target}, optMeta});
                }
                expect(TokenType::Symbol, ";");
            }
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::WAIT_INPUT, {}, {}});
        }
        else if (match(TokenType::Keyword, "halt")) {
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::HALT, {}, {}});
        }
        else if (match(TokenType::Keyword, "local")) {
            std::string var = consume().value;
            expect(TokenType::Symbol, "=");
            Operand res = ParseExpression(p.bytecode);
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::SET_LOCAL, {var, res.val}, {}});
        }
        else if (match(TokenType::Keyword, "wait")) {
            std::string type = consume().value;
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::WAIT_ACTION, {type}, {}});
        }
        else if (match(TokenType::Keyword, "shake")) {
            Operand intensity = ParseExpression(p.bytecode);
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "shake"; j["intensity"] = std::stof(intensity.val);
            p.bytecode.push_back({BitOp::EVENT, {"shake"}, j});
        }
        else if (match(TokenType::Keyword, "delay")) {
            Operand dur = ParseExpression(p.bytecode);
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "delay"; j["duration"] = std::stoi(dur.val);
            p.bytecode.push_back({BitOp::EVENT, {"delay"}, j});
        }
        else if (match(TokenType::Keyword, "play_sfx")) {
            // play_sfx id;
            std::string id = consume().value;
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "play_sfx"; j["id"] = id;
            p.bytecode.push_back({BitOp::EVENT, {"play_sfx"}, j});
        }
        else if (match(TokenType::Keyword, "expression")) {
            // expression target, id;
            std::string target = consume().value;
            expect(TokenType::Symbol, ",");
            std::string id = consume().value;
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "expression"; j["target"] = target; j["id"] = id;
            p.bytecode.push_back({BitOp::EVENT, {"expression"}, j});
        }
        else if (match(TokenType::Keyword, "hide")) {
            // hide target;
            std::string target = consume().value;
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "hide"; j["target"] = target;
            p.bytecode.push_back({BitOp::EVENT, {"hide"}, j});
        }
        else if (match(TokenType::Keyword, "pos")) {
            // pos target, x;
            std::string target = consume().value;
            expect(TokenType::Symbol, ",");
            std::string x = consume().value;
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "pos"; j["target"] = target; j["x"] = x;
            p.bytecode.push_back({BitOp::EVENT, {"pos"}, j});
        }
        else if (match(TokenType::Keyword, "clear")) {
            // clear;  — remove all entities from scene
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "clear";
            p.bytecode.push_back({BitOp::EVENT, {"clear"}, j});
        }
        else if (match(TokenType::Keyword, "random")) {
            // random var, min, max;
            std::string var = consume().value;
            expect(TokenType::Symbol, ",");
            Operand lo = ParseExpression(p.bytecode);
            expect(TokenType::Symbol, ",");
            Operand hi = ParseExpression(p.bytecode);
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "random"; j["var"] = var;
            j["min"] = std::stoi(lo.val); j["max"] = std::stoi(hi.val);
            p.bytecode.push_back({BitOp::EVENT, {"random"}, j});
        }
        else if (match(TokenType::Keyword, "fade")) {
            // fade target, alpha/id, duration;
            std::string target = consume().value;
            expect(TokenType::Symbol, ",");
            std::string valOrId = consume().value;
            expect(TokenType::Symbol, ",");
            std::string dur = consume().value;
            expect(TokenType::Symbol, ";");
            
            nlohmann::json j; j["op"] = "fade"; j["target"] = target;
            if (target == "bg") j["id"] = valOrId;
            else j["alpha"] = std::stof(valOrId);
            j["duration"] = std::stoi(dur);
            p.bytecode.push_back({BitOp::EVENT, {"fade"}, j});
        }
        else if (match(TokenType::Keyword, "move")) {
            // move target, x, duration;
            std::string target = consume().value;
            expect(TokenType::Symbol, ",");
            std::string x = consume().value;
            expect(TokenType::Symbol, ",");
            std::string dur = consume().value;
            expect(TokenType::Symbol, ";");
            
            nlohmann::json j; j["op"] = "move"; j["target"] = target;
            j["x"] = x; j["duration"] = std::stoi(dur);
            p.bytecode.push_back({BitOp::EVENT, {"move"}, j});
        }
        else if (match(TokenType::Keyword, "fade_screen")) {
            std::string alpha = consume().value;
            expect(TokenType::Symbol, ",");
            std::string dur = consume().value;
            expect(TokenType::Symbol, ";");
            nlohmann::json j; j["op"] = "fade_screen"; j["alpha"] = std::stof(alpha); j["duration"] = std::stoi(dur);
            p.bytecode.push_back({BitOp::EVENT, {"fade_screen"}, j});
        }
        else if (match(TokenType::Keyword, "if")) {
            ParseIfStatement(p.bytecode);
        }
        else if (peek().type == TokenType::Identifier && peekNext().value == "=") {
            std::string var = consume().value;
            consume(); // =
            Operand res = ParseExpression(p.bytecode);
            expect(TokenType::Symbol, ";");
            if (res.isRef) p.bytecode.push_back({BitOp::SET_REF, {var, res.val}, {}});
            else p.bytecode.push_back({BitOp::SET, {var, res.val}, {}});
        }
        else if (match(TokenType::Keyword, "bg")) {
            expect(TokenType::Symbol, "=");
            std::string val = consume().value;
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::BG, {val}, {}});
        }
        else if (match(TokenType::Keyword, "bgm")) {
            expect(TokenType::Symbol, "=");
            std::string val = consume().value;
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::BGM, {val}, {}});
        }
        else if (match(TokenType::Keyword, "jump")) {
            std::string target = consume().value;
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::GOTO, {target}, {}});
        }
        else if (match(TokenType::Keyword, "call")) {
            std::string label = consume().value;
            std::vector<std::string> args = { label };
            if (match(TokenType::Symbol, "(")) {
                while (!match(TokenType::Symbol, ")")) {
                    Operand arg = ParseExpression(p.bytecode);
                    args.push_back(arg.val);
                    match(TokenType::Symbol, ",");
                }
            }
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::CALL, args, {}});
        }
        else if (match(TokenType::Keyword, "return")) {
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::RETURN, {}, {}});
        }
        else if (match(TokenType::Keyword, "halt")) {
            expect(TokenType::Symbol, ";");
            p.bytecode.push_back({BitOp::HALT, {}, {}});
        }
        else consume();
    }
    expect(TokenType::Symbol, ";");
}

void BitScriptParser::ParseIfStatement(std::vector<BitInstruction>& output) {
    expect(TokenType::Symbol, "(");
    
    struct Cond { std::string var, op, val; bool isRef; bool isOr; };
    std::vector<Cond> conditions;

    auto parseOne = [&]() {
        std::string var = consume().value;
        std::string op = consume().value;
        Operand val = ParseExpression(output);
        conditions.push_back({var, op, val.val, val.isRef, false});
    };

    parseOne();
    while (match(TokenType::Keyword, "and") || match(TokenType::Keyword, "or")) {
        bool isOr = (previous().value == "or");
        parseOne();
        conditions.back().isOr = isOr;
    }
    
    expect(TokenType::Symbol, ")");
    expect(TokenType::Symbol, "{");
    
    std::string thenLabel = genTempVar() + "_then";
    std::string endLabel = genTempVar() + "_end";

    // Simplified logic for compiling complex conditions without a full expression tree:
    // This is basically a linear chain.
    for (size_t i = 0; i < conditions.size(); ++i) {
        auto& c = conditions[i];
        if (c.isRef) output.push_back({BitOp::IF_REF, {c.var, c.op, c.val, thenLabel}, {}});
        else output.push_back({BitOp::IF, {c.var, c.op, c.val, thenLabel}, {}});
        
        if (i < conditions.size() - 1 && conditions[i+1].isOr) {
            // If the NEXT one is OR, and we didn't jump to 'then', we just continue to check the next one
        } else if (i < conditions.size() - 1) {
            // If the NEXT one is AND, and we didn't jump to 'then', we might need to skip the block if this one was required
        }
    }
    
    // Fallback: if none of the IFs jumped to thenLabel, we GOTO end
    output.push_back({BitOp::GOTO, {endLabel}, {}});
    output.push_back({BitOp::LABEL, {thenLabel}, {}});
    
    while (!match(TokenType::Symbol, "}")) {
        if (match(TokenType::Keyword, "jump")) {
            output.push_back({BitOp::GOTO, {consume().value}, {}});
            expect(TokenType::Symbol, ";");
        } else if (peek().type == TokenType::Identifier && peekNext().value == "=") {
            std::string var = consume().value;
            consume(); // =
            Operand res = ParseExpression(output);
            expect(TokenType::Symbol, ";");
            if (res.isRef) output.push_back({BitOp::SET_REF, {var, res.val}, {}});
            else output.push_back({BitOp::SET, {var, res.val}, {}});
        } else consume();
    }
    output.push_back({BitOp::LABEL, {endLabel}, {}});
    expect(TokenType::Symbol, ";");
}

Operand BitScriptParser::ParseExpression(std::vector<BitInstruction>& output) {
    return ParseAddExpr(output);
}

Operand BitScriptParser::ParseAddExpr(std::vector<BitInstruction>& output) {
    Operand left = ParseMulExpr(output);
    while (match(TokenType::Symbol, "+") || match(TokenType::Symbol, "-")) {
        std::string op = previous().value;
        Operand right = ParseMulExpr(output);
        std::string tmp = genTempVar();
        if (left.isRef) output.push_back({BitOp::SET_REF, {tmp, left.val}, {}});
        else output.push_back({BitOp::SET, {tmp, left.val}, {}});
        
        BitOp bop = (op == "+") ? (right.isRef ? BitOp::ADD_REF : BitOp::ADD) : (right.isRef ? BitOp::SUB_REF : BitOp::SUB);
        output.push_back({bop, {tmp, right.val}, {}});
        left = {true, tmp};
    }
    return left;
}

Operand BitScriptParser::ParseMulExpr(std::vector<BitInstruction>& output) {
    Operand left = ParsePrimary();
    while (match(TokenType::Symbol, "*") || match(TokenType::Symbol, "/")) {
        std::string op = previous().value;
        Operand right = ParsePrimary();
        std::string tmp = genTempVar();
        if (left.isRef) output.push_back({BitOp::SET_REF, {tmp, left.val}, {}});
        else output.push_back({BitOp::SET, {tmp, left.val}, {}});
        
        BitOp bop = (op == "*") ? (right.isRef ? BitOp::MUL_REF : BitOp::MUL) : (right.isRef ? BitOp::DIV_REF : BitOp::DIV);
        output.push_back({bop, {tmp, right.val}, {}});
        left = {true, tmp};
    }
    return left;
}

Operand BitScriptParser::ParsePrimary() {
    if (match(TokenType::Number)) return {false, previous().value};
    if (match(TokenType::Identifier)) return {true, previous().value};
    if (match(TokenType::Keyword, "true")) return {false, "1"};
    if (match(TokenType::Keyword, "false")) return {false, "0"};
    return {false, "0"};
}

// --- Interpreter Interface ---

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
