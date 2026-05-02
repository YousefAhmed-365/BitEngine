# 🧩 BitScript Specification & Documentation
**Version 0.2**

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

SceneBlock          = "scene" Identifier [ "(" { Identifier } ")" ] "{" { SceneStatement } "}" ";" ;
SceneStatement      = Dialogue
                    | ChoiceBlock
                    | Assignment
                    | IfStatement
                    | JumpStatement
                    | JoinStatement
                    | LeaveStatement
                    | StackStatement
                    | TransitionStatement
                    | UiStatement
                    | NarrationStatement
                    | ExpressionStatement ;

Dialogue            = Identifier [ "." Identifier ] [ "[" { Modifier } "]" ] ":" String ";" ;
NarrationStatement  = "narration" [ "[" { Modifier } "]" ] ":" String ";" ;
JoinStatement       = ">" Identifier [ "[" { Modifier } "]" ] ";" ;
LeaveStatement      = "<" Identifier ";" ;
StackStatement      = "call" Identifier [ "(" { Expression } ")" ] ";" | "return" ";" ;
TransitionStatement = "transition" Identifier "," Number "," Number ";" ;
UiStatement         = "ui" ("show" | "hide") ";" ;
Modifier            = Identifier "=" Expression ;

ChoiceBlock         = "choice" "{" { ChoiceOption } "}" ";" ;
ChoiceOption        = String "->" Identifier [ "if" Expression ] ";" ;

JumpStatement       = "jump" Identifier ";" ;
IfStatement         = "if" "(" Expression ")" "{" { SceneStatement } "}" ";" ;
LocalDecl           = "local" Identifier "=" Expression ";" ;
WaitStatement       = "wait" ("move" | "fade" | "all" | "sfx") ";" ;
StackStatement      = "call" Identifier [ "(" { Expression } ")" ] ";" | "return" ";" ;

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

        alias angry {
            sprite = serious,
            shake = true
        };
    };
};
```

### 3. Scenes & Dialogue
Scenes support parameters and character **aliases** (moods).
```bitscript
scene intro_scene(day_count) {
    akira.angry: "It's already been {day_count} days!";
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

if (trust >= 10 and visited_market == true) {
    jump good_ending;
};
```

### 6. Narrative Stack & Parameters
Scenes can be called like functions with parameters and their own local scope.
- `local <var> = <val>;`: Defines a variable only visible within the current scene.
- `call <scene>(<args>)`: Jumps to a scene and passes values to its parameters.

```bitscript
scene market {
    local price = 50;
    call pay_routine(price);
}

scene pay_routine(amount) {
    narration: "You paid {amount} credits.";
    return;
}
```

### 7. Character Management & Aliases
- `> <id> [modifiers]`: Join character to scene.
- `< <id>`: Remove character from scene.
- `<id>.<alias>`: Use a predefined mood alias from the entity definition.

```bitscript
akira.angry: "This is taking too long!";
```

### 8. Cinematic Statements & Wait
- `transition <fade_type>, <duration>, <post_delay>;`
- `ui <show/hide>;`
- `wait <move|fade|all|sfx>;`: Pauses the script until the specified action completes.

```bitscript
fade akira, 1.0, 1000;
wait fade; # Script will not proceed until Akira is visible
akira: "I'm here.";
```

### 9. Comments
BitScript supports both single and multi-line comments.
```bitscript
# This is a single line comment

#-
  This is a multi-line
  comment block.
-#
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

---

## 🔍 Static Analyzer (New in v0.2)

BitScript includes a built-in static analyzer that proactively identifies logic and narrative errors during the compilation phase.

### 1. Referential Integrity
The analyzer verifies that every `jump`, `call`, and `choice` target points to a valid `scene` label. This prevents "missing label" crashes during gameplay.

### 2. Variable Auditing
- **Undefined Usage**: Detects when a script attempts to read from or write to a variable that hasn't been declared in a global `var` block.
- **Conditional Integrity**: Ensures that `if` statements and choice conditions reference valid state registers.

### 3. Entity & Asset Verification
- **Character Registry**: Ensures that dialogue statements and `join`/`leave` commands use valid character IDs defined in the `entities` block.
- **Alias Verification**: Validates that character moods (`akira.angry`) correspond to aliases defined in the entity's configuration.

### 4. Line-Linked Reporting
Errors and warnings are mapped directly to their source line numbers, allowing developers to quickly locate and resolve issues.

```text
[ERROR] (Line 142): Duplicate scene label: intro_scene
[WARN] (Line 85): Assignment to undefined variable: player_trust
```
