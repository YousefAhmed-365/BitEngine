#ifndef BIT_OP_HPP
#define BIT_OP_HPP

#include "json.hpp"
#include <string>
#include <vector>

// Bytecode operation codes
enum class BitOp {
    // Narrative
    TEXT,           // entity, content (non-blocking)
    SAY,            // entity, content (blocking)
    CHOICE,         // text, target_label
    
    // Control Flow
    IF,             // var, op, value, target_label
    IF_REF,         // var, op, ref_var, target_label
    GOTO,           // target_label
    LABEL,          // marker (no-op)
    CALL,           // subroutine call
    RETURN,         // subroutine return
    
    // Variable Operations
    SET,            // var, val
    SET_REF,        // var, ref_var
    ADD,            // var, val
    ADD_REF,        // var, ref_var
    SUB,
    SUB_REF,
    MUL,
    MUL_REF,
    DIV,
    DIV_REF,
    SET_LOCAL,      // local variable
    
    // Animation & Transitions
    TRANSITION,     // transition effect
    BG,             // background id
    BGM,            // music id
    SFX,            // one-shot sound effect
    
    // UI Commands
    UI_VISIBLE,     // show/hide UI
    UI_LOAD,        // name, path, layer
    UI_UNLOAD,      // name
    UI_ACTIVATE,    // name, (optional) layer
    UI_DEACTIVATE,  // name
    UI_SET,         // scoped_id, property, value
    
    // Events & Timeline
    EVENT,          // op, params_json
    EMIT,           // event name
    WAIT_EVENT,     // wait for event
    PLAY_TIMELINE,  // timeline id, wait
    
    // Input & Delays
    WAIT_INPUT,     // wait for player input
    WAIT_ACTION,    // wait for action (animation, etc)
    
    // Termination
    HALT            // end execution
};

struct BitInstruction {
    BitOp op;
    std::vector<std::string> args;
    nlohmann::json metadata;
    int line = -1;
};

#endif
