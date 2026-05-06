#include "BitParser.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>

void BitParser::ParseStatement() {
    if (match(TokenType::Keyword, "scene")) ParseScene();
    else if (match(TokenType::Keyword, "timeline")) ParseTimeline();
    else if (match(TokenType::Keyword, "leave")) {
        std::string entityId = consume().value;
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "leave"; j["target"] = entityId;
        emit(BitOp::EVENT, {"leave"}, j);
    }
    else if (match(TokenType::Keyword, "ui")) {
        std::string action = peek().value; consume();
        if (action == "show" || action == "hide") {
            // ui show; / ui hide; — optional scoped id after action
            if (peek().type == TokenType::Identifier) {
                std::string id = consume().value;
                expect(TokenType::Symbol, ";");
                std::string val = (action == "hide" ? "false" : "true");
                emit(BitOp::UI_SET, {id, "visible", val});
            } else {
                expect(TokenType::Symbol, ";");
                emit(BitOp::UI_VISIBLE, {action});
            }
        } else if (action == "activate") {
            std::string name = consume().value;
            std::vector<std::string> args = { name };
            if (peek().type == TokenType::Number) args.push_back(consume().value);
            expect(TokenType::Symbol, ";");
            emit(BitOp::UI_ACTIVATE, args);
        } else if (action == "deactivate") {
            std::string name = consume().value;
            expect(TokenType::Symbol, ";");
            emit(BitOp::UI_DEACTIVATE, {name});
        } else if (action == "load") {
            std::string name = consume().value;
            std::string path = consume().value; // string token
            std::string layer = "0";
            if (peek().type == TokenType::Number) layer = consume().value;
            expect(TokenType::Symbol, ";");
            emit(BitOp::UI_LOAD, {name, path, layer});
        } else if (action == "unload") {
            std::string name = consume().value;
            expect(TokenType::Symbol, ";");
            emit(BitOp::UI_UNLOAD, {name});
        } else if (action == "set") {
            std::string name = consume().value;
            std::string scopedId = name;
            if (match(TokenType::Symbol, ".")) scopedId += "." + consume().value;
            std::string prop = consume().value;
            Operand val = ParseExpression(*m_currentOutput);
            expect(TokenType::Symbol, ";");
            emit(BitOp::UI_SET, {scopedId, prop, val.val});
        } else {
            p.parseErrors.push_back("Parse error line " + std::to_string(previous().line) +
                ": Unknown ui sub-command '" + action + "'. Valid: show, hide, activate, deactivate, load, unload, set");
        }
    }
    else if (match(TokenType::Keyword, "ui_load") || match(TokenType::Keyword, "ui_unload") ||
             match(TokenType::Keyword, "ui_set")  || match(TokenType::Keyword, "ui_activate") ||
             match(TokenType::Keyword, "ui_deactivate")) {
        p.parseErrors.push_back("Parse error line " + std::to_string(previous().line) +
            ": '" + previous().value + "' has been removed. Use 'ui <action> ...' syntax instead.");
    }
    else if (match(TokenType::Keyword, "narration")) {
        nlohmann::json meta;
        if (match(TokenType::Symbol, "[")) {
            while (peek().type != TokenType::EndOfFile && peek().value != "]") {
                std::string k = consume().value;
                expect(TokenType::Symbol, "=");
                std::string v = consume().value;
                if (k == "pre_delay" || k == "duration" || k == "wait") {
                    meta[k] = ParseTime(v);
                } else {
                    meta[k] = v;
                }
                match(TokenType::Symbol, ",");
            }
            expect(TokenType::Symbol, "]");
        }
        match(TokenType::Symbol, ":");
        std::string text = consume().value;
        expect(TokenType::Symbol, ";");
        emit(BitOp::SAY, {"narration", text}, meta);
    }
    else if (peek().type == TokenType::Identifier && (
        peekNext().value == ":" || peekNext().value == "[" || peekNext().value == "." ||
        peekNext().value == "=" || peekNext().value == "{" ||
        peekNext().value == "+=" || peekNext().value == "-=" ||
        peekNext().value == "*=" || peekNext().value == "/=" ||
        peekNext().value == "++" || peekNext().value == "--")) {
        std::string entity = consume().value;
        // Assignment or compound assignment
        if (match(TokenType::Symbol, "=") ||
            peek().value == "+=" || peek().value == "-=" ||
            peek().value == "*=" || peek().value == "/=" ||
            peek().value == "++" || peek().value == "--") {
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
                if (k == "pre_delay" || k == "duration" || k == "wait") {
                    meta[k] = ParseTime(v);
                } else {
                    meta[k] = v;
                }
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
                    if (k == "pre_delay" || k == "duration" || k == "wait") {
                        optMeta[k] = ParseTime(v);
                    } else {
                        optMeta[k] = v;
                    }
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
        nlohmann::json j; j["op"] = "shake"; 
        if (intensity.isRef) j["intensity"] = intensity.val;
        else try { j["intensity"] = std::stof(intensity.val); } catch(...) { j["intensity"] = 5.0f; }
        emit(BitOp::EVENT, {"shake"}, j);
        if (wait) emit(BitOp::WAIT_ACTION, {"all"});
    }
    else if (match(TokenType::Keyword, "delay")) {
        Operand dur = ParseExpression(*m_currentOutput);
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "delay"; 
        if (dur.isRef) j["duration"] = dur.val;
        else j["duration"] = ParseTime(dur.val);
        emit(BitOp::EVENT, {"delay"}, j);
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
        std::string x_str;
        Operand x = {false, "0.5"};
        if (peek().type == TokenType::Identifier && (peek().value == "left" || peek().value == "right" || peek().value == "center")) {
            x_str = consume().value;
            x.val = x_str;
        } else {
            x = ParseExpression(*m_currentOutput);
        }
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "pos"; j["target"] = target; 
        if (x.isRef) j["x"] = x.val;
        else j["x"] = x.val;
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
        std::string target = ParseAssetId();

        nlohmann::json j; j["op"] = "fade"; j["target"] = target;
        if (target == "bg") {
            std::string bgId = ParseAssetId();
            j["id"] = bgId;
        } else {
            // alpha: bare number/id or (expr)
            Operand alpha = ParseCinematicArg();
            if (alpha.isRef) j["alpha"] = alpha.val;
            else try { j["alpha"] = std::stof(alpha.val); } catch(...) { j["alpha"] = -1.0f; }
        }

        Operand dur = ParseCinematicArg();
        bool wait = match(TokenType::Keyword, "wait");
        expect(TokenType::Symbol, ";");

        if (dur.isRef) j["duration"] = dur.val;
        else j["duration"] = ParseTime(dur.val);

        emit(BitOp::EVENT, {"fade"}, j);
        if (wait) emit(BitOp::WAIT_ACTION, {"fade"});
    }
    else if (match(TokenType::Keyword, "move")) {
        std::string target = ParseAssetId();

        Operand x = {false, "0.5"};
        if (peek().type == TokenType::Identifier &&
            (peek().value == "left" || peek().value == "right" || peek().value == "center")) {
            x.val = consume().value;
        } else {
            x = ParseCinematicArg();
        }
        Operand dur = ParseCinematicArg();
        bool wait = match(TokenType::Keyword, "wait");
        expect(TokenType::Symbol, ";");

        nlohmann::json j; j["op"] = "move"; j["target"] = target;
        j["x"] = x.val;
        if (dur.isRef) j["duration"] = dur.val;
        else j["duration"] = ParseTime(dur.val);

        emit(BitOp::EVENT, {"move"}, j);
        if (wait) emit(BitOp::WAIT_ACTION, {"move"});
    }
    else if (match(TokenType::Keyword, "fade_screen")) {
        Operand alpha = ParseCinematicArg();
        Operand dur   = ParseCinematicArg();
        bool wait = match(TokenType::Keyword, "wait");
        expect(TokenType::Symbol, ";");

        nlohmann::json j; j["op"] = "fade_screen";
        if (alpha.isRef) j["alpha"] = alpha.val;
        else try { j["alpha"] = std::stof(alpha.val); } catch(...) { j["alpha"] = -1.0f; }
        if (dur.isRef) j["duration"] = dur.val;
        else j["duration"] = ParseTime(dur.val);

        emit(BitOp::EVENT, {"fade_screen"}, j);
        if (wait) emit(BitOp::WAIT_ACTION, {"fade"});
    }
    else if (match(TokenType::Keyword, "bg")) {
        // bg id [fade = 1s]; or bg {var};
        std::string val = ParseAssetId();
        nlohmann::json j; j["op"] = "bg"; j["id"] = val;
        if (match(TokenType::Symbol, "[")) {
            while (peek().type != TokenType::EndOfFile && peek().value != "]") {
                std::string k = consume().value;
                expect(TokenType::Symbol, "=");
                std::string v = consume().value;
                if (k == "fade") j["fade"] = ParseTime(v);
                match(TokenType::Symbol, ",");
            }
            expect(TokenType::Symbol, "]");
        }
        expect(TokenType::Symbol, ";");
        if (!j.contains("fade")) {
            emit(BitOp::BG, {val});
        } else {
            // Emit fade event then bg swap
            nlohmann::json fj; fj["op"] = "fade"; fj["target"] = "bg"; fj["id"] = val;
            fj["duration"] = j["fade"];
            emit(BitOp::EVENT, {"fade"}, fj);
        }
    }
    else if (match(TokenType::Keyword, "bgm")) {
        std::string val = ParseAssetId();
        nlohmann::json meta;
        if (match(TokenType::Symbol, "[")) {
            while (peek().type != TokenType::EndOfFile && peek().value != "]") {
                std::string k = consume().value;
                expect(TokenType::Symbol, "=");
                std::string v = consume().value;
                if (k == "fade") meta[k] = ParseTime(v);
                else             meta[k] = v;
                match(TokenType::Symbol, ",");
            }
            expect(TokenType::Symbol, "]");
        }
        expect(TokenType::Symbol, ";");
        emit(BitOp::BGM, {val}, meta);
    }
    else if (match(TokenType::Keyword, "sfx")) {
        std::string val = ParseAssetId();
        expect(TokenType::Symbol, ";");
        nlohmann::json j; j["op"] = "play_sfx"; j["id"] = val;
        emit(BitOp::SFX, {val});
    }
    else if (match(TokenType::Keyword, "emit")) {
        std::string target = consume().value;
        if (target.size() >= 2 && target.front() == '"' && target.back() == '"') {
            target = target.substr(1, target.size() - 2);
        }
        expect(TokenType::Symbol, ";");
        emit(BitOp::EMIT, {target});
    }
    else if (match(TokenType::Keyword, "wait")) {
        if (match(TokenType::Keyword, "event")) {
            std::string target = consume().value;
            if (target.size() >= 2 && target.front() == '"' && target.back() == '"') {
                target = target.substr(1, target.size() - 2);
            }
            expect(TokenType::Symbol, ";");
            emit(BitOp::WAIT_EVENT, {target});
        } else {
            consume();
        }
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
        consume();
    }
}

void BitParser::ParseAssignment(const std::string& var, std::vector<BitInstruction>& output, bool isLocal) {
    // ++ / --
    if (peek().value == "++" || peek().value == "--") {
        bool inc = consume().value == "++";
        expect(TokenType::Symbol, ";");
        emit(inc ? BitOp::ADD : BitOp::SUB, {var, "1"});
        return;
    }
    // += -= *= /=
    if (peek().value == "+=" || peek().value == "-=" || peek().value == "*=" || peek().value == "/=") {
        std::string op = consume().value;
        Operand right = ParseExpression(output);
        expect(TokenType::Symbol, ";");
        BitOp bop;
        if      (op == "+=") bop = right.isRef ? BitOp::ADD_REF : BitOp::ADD;
        else if (op == "-=") bop = right.isRef ? BitOp::SUB_REF : BitOp::SUB;
        else if (op == "*=") bop = right.isRef ? BitOp::MUL_REF : BitOp::MUL;
        else                 bop = right.isRef ? BitOp::DIV_REF : BitOp::DIV;
        emit(bop, {var, right.val});
        return;
    }
    // var op= var (legacy: var = var + val)
    if (peek().type == TokenType::Identifier && peek().value == var &&
        (peekNext().value == "+" || peekNext().value == "-" || peekNext().value == "*" || peekNext().value == "/")) {
        consume();
        std::string op = consume().value;
        Operand right = ParseExpression(output);
        expect(TokenType::Symbol, ";");
        BitOp bop;
        if      (op == "+") bop = right.isRef ? BitOp::ADD_REF : BitOp::ADD;
        else if (op == "-") bop = right.isRef ? BitOp::SUB_REF : BitOp::SUB;
        else if (op == "*") bop = right.isRef ? BitOp::MUL_REF : BitOp::MUL;
        else                bop = right.isRef ? BitOp::DIV_REF : BitOp::DIV;
        emit(bop, {var, right.val});
        return;
    }

    Operand res = ParseExpression(output);
    expect(TokenType::Symbol, ";");
    if (isLocal) {
        emit(BitOp::SET_LOCAL, {var, res.val});
    } else {
        if (res.isRef) emit(BitOp::SET_REF, {var, res.val});
        else           emit(BitOp::SET, {var, res.val});
    }
}


// ─── ParseAssetId ────────────────────────────────────────────────────────────
// Accepts: bare identifier, "string" literal, or {var} dynamic reference.
// Returns the raw string value; for {var} the value is prefixed with "@" so the
// VM knows to resolve it at runtime.
std::string BitParser::ParseAssetId() {
    if (peek().type == TokenType::Symbol && peek().value == "{") {
        consume(); // {
        std::string inner = consume().value; // var name or string literal
        expect(TokenType::Symbol, "}");
        return "@" + inner; // "@" prefix = dynamic reference
    }
    if (peek().type == TokenType::String) return consume().value;
    return consume().value; // bare identifier
}

// ─── ParseCinematicArg ───────────────────────────────────────────────────────
// Accepts: bare Number/Identifier literal, OR (expr) for any expression.
Operand BitParser::ParseCinematicArg() {
    if (peek().type == TokenType::Symbol && peek().value == "(") {
        consume(); // (
        Operand result = ParseExpression(*m_currentOutput);
        expect(TokenType::Symbol, ")");
        return result;
    }
    Token t = consume();
    bool isRef = (t.type == TokenType::Identifier);
    return {isRef, t.value};
}

void BitParser::ParseDialogueBlock(const std::string& entityId, std::vector<BitInstruction>& output) {
    while (peek().type != TokenType::EndOfFile && peek().value != "}") {
        // .cmd or .alias inside entity block
        if (peek().value == ".") {
            consume(); // .
            std::string cmd = consume().value;

            // cinematic commands scoped to this entity
            if (cmd == "move") {
                Operand x = {false, "0.5"};
                if (peek().type == TokenType::Identifier &&
                    (peek().value == "left" || peek().value == "right" || peek().value == "center")) {
                    x.val = consume().value;
                } else {
                    x = ParseCinematicArg();
                }
                Operand dur = ParseCinematicArg();
                bool wait = match(TokenType::Keyword, "wait");
                expect(TokenType::Symbol, ";");
                nlohmann::json j; j["op"] = "move"; j["target"] = entityId;
                j["x"] = x.val;
                if (dur.isRef) j["duration"] = dur.val;
                else j["duration"] = ParseTime(dur.val);
                emit(BitOp::EVENT, {"move"}, j);
                if (wait) emit(BitOp::WAIT_ACTION, {"move"});
                continue;
            }
            if (cmd == "fade") {
                Operand alpha = ParseCinematicArg();
                Operand dur   = ParseCinematicArg();
                bool wait = match(TokenType::Keyword, "wait");
                expect(TokenType::Symbol, ";");
                nlohmann::json j; j["op"] = "fade"; j["target"] = entityId;
                if (alpha.isRef) j["alpha"] = alpha.val;
                else try { j["alpha"] = std::stof(alpha.val); } catch(...) { j["alpha"] = 1.0f; }
                if (dur.isRef) j["duration"] = dur.val;
                else j["duration"] = ParseTime(dur.val);
                emit(BitOp::EVENT, {"fade"}, j);
                if (wait) emit(BitOp::WAIT_ACTION, {"fade"});
                continue;
            }

            // .alias or .alias: "text"
            nlohmann::json meta;
            meta["alias"] = cmd;
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
            if (match(TokenType::Symbol, ":")) {
                std::string text = consume().value;
                expect(TokenType::Symbol, ";");
                emit(BitOp::SAY, {entityId, text}, meta);
            } else {
                // alias-only: apply modifiers without dialogue
                expect(TokenType::Symbol, ";");
                emit(BitOp::SAY, {entityId, ""}, meta);
            }
            continue;
        }

        // bare ":" — dialogue with no alias
        if (peek().value == ":") {
            consume();
            std::string text = consume().value;
            expect(TokenType::Symbol, ";");
            emit(BitOp::SAY, {entityId, text}, {});
            continue;
        }

        // string directly
        if (peek().type == TokenType::String) {
            std::string text = consume().value;
            expect(TokenType::Symbol, ";");
            emit(BitOp::SAY, {entityId, text}, {});
            continue;
        }

        consume(); // skip unknown
    }
    expect(TokenType::Symbol, "}");
    match(TokenType::Symbol, ";");
}

Operand BitParser::ParseExpression(std::vector<BitInstruction>& output) {
    return ParseAddExpr(output);
}

Operand BitParser::ParseAddExpr(std::vector<BitInstruction>& output) {
    Operand left = ParseMulExpr(output);
    while (peek().type != TokenType::EndOfFile && (peek().value == "+" || peek().value == "-")) {
        std::string op = consume().value;
        Operand right = ParseMulExpr(output);
        
        if (!left.isRef && !right.isRef) {
            try {
                float lval = std::stof(left.val);
                float rval = std::stof(right.val);
                float result = (op == "+") ? (lval + rval) : (lval - rval);
                if (result == std::floor(result)) left.val = std::to_string((int)result);
                else left.val = std::to_string(result);
            } catch (...) {
                std::cerr << "[BitScript] Parse error: Cannot perform math on non-numbers\n";
            }
        } else {
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
            } else {
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
            left = {true, resultVar};
        }
    }
    return left;
}

Operand BitParser::ParseMulExpr(std::vector<BitInstruction>& output) {
    Operand left = ParsePrimary();
    while (peek().type != TokenType::EndOfFile && (peek().value == "*" || peek().value == "/")) {
        std::string op = consume().value;
        Operand right = ParsePrimary();
        
        if (!left.isRef && !right.isRef) {
            try {
                float lval = std::stof(left.val);
                float rval = std::stof(right.val);
                if (op == "/" && rval == 0.0f) {
                    left.val = "0";
                } else {
                    float result = (op == "*") ? (lval * rval) : (lval / rval);
                    if (result == std::floor(result)) left.val = std::to_string((int)result);
                    else left.val = std::to_string(result);
                }
            } catch (...) {
                std::cerr << "[BitScript] Parse error: Cannot perform math\n";
            }
        } else {
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
            } else {
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
            left = {true, resultVar};
        }
    }
    return left;
}

Operand BitParser::ParsePrimary() {
    Token t = consume();

    if (t.type == TokenType::Symbol && t.value == "(") {
        Operand inside = ParseExpression(*m_currentOutput);
        expect(TokenType::Symbol, ")");
        return inside;
    }

    if (t.type == TokenType::Number) {
        if ((t.value.size() > 2 && t.value.substr(t.value.size() - 2) == "ms") ||
            (t.value.size() > 1 && t.value.back() == 's')) {
            return {false, std::to_string(ParseTime(t.value))};
        }
        return {false, t.value};
    }

    if (t.type == TokenType::Identifier) {
        if (p.variables.find(t.value) == p.variables.end()) {
            std::cerr << "[BitScript] Parse error: Undefined variable '" << t.value << "'\n";
        }
        return {true, t.value};
    }

    if (t.type == TokenType::String) return {false, t.value};

    return {false, "0"};
}

void BitParser::ParseIfStatement(std::vector<BitInstruction>& output) {
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
        consume();
        parseOne();
    }
    
    expect(TokenType::Symbol, ")");
    expect(TokenType::Symbol, "{");
    
    std::string endLabel = genTempVar() + "_end";

    auto negateOp = [](const std::string& op) -> std::string {
        if (op == "==" || op == "=") return "!=";
        if (op == "!=") return "==";
        if (op == ">") return "<=";
        if (op == "<") return ">=";
        if (op == ">=") return "<";
        if (op == "<=") return ">";
        return op;
    };

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
