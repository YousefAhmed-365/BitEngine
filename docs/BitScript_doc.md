# 🧩 BitScript Specification & Documentation
**Version 0.1**

BitScript is a lightweight, high-performance narrative scripting language designed specifically for the **BitEngine VM**. It combines declarative entity management with a powerful expression-native logic system to enable complex branching storytelling with minimal syntax overhead.

---

## 🎮 Language Overview

A BitScript project is composed of four primary global blocks:
1.  **`config`**: Global engine settings and project metadata.
2.  **`var`**: Global state variables (persistent across saves).
3.  **`entities`**: Definitions for characters, system actors, and props.
4.  **`scene`**: The core execution nodes containing dialogue and logic.

---

## ⚙️ Core Structure (EBNF)

```ebnf
Program             = { GlobalStatement } ;

GlobalStatement     = ConfigBlock
                    | VariableDecl
                    | EntityBlock
                    | SceneBlock
                    | ";" ;

ConfigBlock         = "config" "{" { ConfigEntry } "}" ";" ;
ConfigEntry         = Identifier "=" Expression ";" ;

VariableDecl        = "var" Identifier "=" Expression ";" ;
Assignment          = Identifier "=" Expression ";" ;

EntityBlock         = "entities" "{" { Entity } "}" ";" ;
Entity              = Identifier "{" { EntityProperty } "}" ";" ;
EntityProperty      = "name" "=" String ";"
                    | "type" "=" Identifier ";"
                    | "default_pos" "=" Position ";"
                    | SpriteBlock ;

SpriteBlock         = "sprite" Identifier "{" { SpriteProperty } "}" ";" ;
SpriteProperty      = "path" "=" String ";"
                    | "frames" "=" Number ";"
                    | "speed" "=" Number ";"
                    | "scale" "=" Number ";" ;

SceneBlock          = "scene" Identifier "{" { SceneStatement } "}" ";" ;
SceneStatement      = Dialogue
                    | ChoiceBlock
                    | Assignment
                    | IfStatement
                    | JumpStatement
                    | ExpressionStatement ;

Dialogue            = Identifier [ "[" { Modifier } "]" ] ":" String ";" ;
Modifier            = Identifier "=" Expression ;

ChoiceBlock         = "choice" "{" { ChoiceOption } "}" ";" ;
ChoiceOption        = String "->" Identifier [ "if" Expression ] ";" ;

JumpStatement       = "jump" Identifier ";" ;
IfStatement         = "if" "(" Expression ")" "{" { SceneStatement } "}" ";" ;

Expression          = LogicalExpr ;
LogicalExpr         = ComparisonExpr { ("and" | "or") ComparisonExpr } ;
ComparisonExpr      = AddExpr [ CompareOp AddExpr ] ;
AddExpr             = MulExpr { ("+" | "-") MulExpr } ;
MulExpr             = UnaryExpr { ("*" | "/") UnaryExpr } ;
UnaryExpr           = [ "-" | "!" ] Primary ;
Primary             = Number | String | Identifier | Boolean | "(" Expression ")" ;

CompareOp           = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
Position            = "left" | "right" | "center" | Number ;
Number              = Digit { Digit } [ "." Digit { Digit } ] ;
String              = "\"" { AnyChar } "\"" ;
Boolean             = "true" | "false" ;
Identifier          = Letter { Letter | Digit | "_" } ;
```

---

## 🎭 Detailed Syntax

### 1. Config Block
Sets engine-level parameters. 
- `start_node`: The ID of the first scene to execute.
- `mode`: Interaction mode (e.g., `typewriter`).
- `reveal_speed`: Characters per second.

```bitscript
config {
    start_node = intro_scene;
    reveal_speed = 45;
};
```

### 2. Entities & Sprites
Entities represent characters. They can have multiple **sprites** mapped to "expressions".
```bitscript
entities {
    akira {
        name = "Akira";
        type = Fixer;
        default_pos = right;

        sprite idle {
            path = "assets/akira_idle.png";
            frames = 2;
            speed = 4.0;
        };
    };
};
```

### 3. Scenes & Dialogue
Scenes are the heart of the narrative. Dialogue is bound to an entity and supports **modifiers** in square brackets.
```bitscript
scene intro_scene {
    akira [sprite=idle; pos=center;]: "We've finally arrived.";
    jump next_scene;
};
```

### 4. Choices
Choices provide interactive branching. They support conditional visibility using the `if` keyword.
```bitscript
choice {
    "Help her" -> help_scene if trust > 5;
    "Walk away" -> leave_scene;
};
```

### 5. Control Flow
BitScript supports standard conditional logic and jumps.
```bitscript
if (trust >= 10 and visited_market == true) {
    jump good_ending;
};
```

---

## 🧮 Expression System

BitScript features a robust expression evaluator used in assignments, conditions, and dialogue modifiers.

| Type | Operators |
| :--- | :--- |
| **Arithmetic** | `+`, `-`, `*`, `/` |
| **Logic** | `and`, `or`, `!` |
| **Comparison** | `==`, `!=`, `<`, `>`, `<=`, `>=` |

**Variables** are dynamically typed (Numbers, Booleans, Strings) and persist in the VM's state registers.

---

## 🚀 Native VM & Bytecode (New in v0.1)

As of version 0.1, BitEngine has moved to a **strictly bytecode-driven architecture**.

### Compilation
The human-readable `.bitscript` files must be compiled into binary `.bitc` files before execution (though the engine can parse source files directly for development).
- **Encoding**: Compiled bytecode is **XOR-encrypted** to protect narrative content.
- **Verification**: The compiler performs static analysis to ensure all labels and jumps are valid.

### Command Line Interface (CLI)
The engine provides a comprehensive CLI for developers:
- `-c, --compile <src> [dst]`: Compiles source to encrypted bytecode.
- `-r, --run <file>`: Executes a `.bitscript` or `.bitc` file.
- `-d, --dry-run <file>`: Validates syntax and logic without opening a window.
- `-s, --stats <file>`: Generates a project summary (scene count, variable usage, etc.).

### Removed Features
- **JSON Narrative Support**: Legacy JSON-based dialog nodes have been removed. All narrative logic now resides within the BitScript VM.
- **Implicit Validation**: Validation is now handled during the compilation phase, reducing runtime overhead.

---

## 🎨 Aesthetic & Rendering
- **Rich Text**: Supports style tags (color, bold, speed) within dialogue strings.
- **Dynamic Transitions**: Scenes support backgrounds, BGM, and SFX with integrated fade logic.
- **VM State**: The engine tracks the current speaker, active entities, and execution history automatically.
