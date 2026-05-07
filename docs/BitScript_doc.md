# 🧩 BitScript Specification & Documentation
**Version 0.3**

BitScript is a lightweight, high-performance narrative scripting language designed specifically for the **BitEngine VM**. It combines declarative entity management with a powerful expression-native logic system to enable complex branching storytelling with minimal syntax overhead.

---

## 🎮 Language Overview

A BitScript project operates alongside a `project.json` file which handles configurations, assets, and entities. The BitScript file itself handles only game logic and contains these global blocks:
1.  **`var`**: Global state variables (persistent across saves).
2.  **`event`**: Named logic blocks that can be triggered externally or via `emit`.
3.  **`timeline`**: Time-precise sequences of commands for complex cutscenes.
4.  **`scene`**: The core execution nodes containing dialogue and logic.

---

## ⚙️ Core Structure (EBNF)

```ebnf
Program             = { GlobalStatement } ;

GlobalStatement     = VariableDecl
                    | EventBlock
                    | TimelineBlock
                    | SceneBlock
                    | [ ";" ] ;

VariableDecl        = "var" Identifier [ "=" Expression ] [ "{" { RangeProp } "}" ] ";" ;
RangeProp           = ("min" | "max") "=" Expression ";" ;

Assignment          = Identifier ( "=" | "+=" | "-=" | "*=" | "/=" | "++" | "--" ) Expression ";" ;



EventBlock          = "event" Identifier "{" { SceneStatement } "}" [ ";" ] ;

TimelineBlock       = "timeline" [ Identifier ] "{" { TimelineEntry } "}" [ ";" ] ;
TimelineEntry       = TimeValue ":" ( SceneStatement | "{" { SceneStatement } "}" ) ;

SceneBlock          = "scene" Identifier [ "(" { Identifier } ")" ] "{" { SceneStatement } "}" [ ";" ] ;
SceneStatement      = Dialogue
                    | ChoiceBlock
                    | Assignment
                    | IfStatement
                    | JumpStatement
                    | JoinStatement
                    | LeaveStatement
                    | StackStatement
                    | CinematicStatement
                    | UiStatement
                    | NarrationStatement
                    | ExpressionStatement
                    | PlayTimelineStatement
                    | EmitStatement
WaitEventStatement    = "wait" "event" String ";" ;
SoundStatement        = ("bg" | "bgm" | "sfx") AssetId [ "[" { Modifier } "]" ] ";" ;

Dialogue            = Identifier [ "." Identifier ] [ "[" { Modifier } "]" ] ":" String ";" 
                    | Identifier "{" { SceneStatement } "}" ;
NarrationStatement  = "narration" [ "[" { Modifier } "]" ] ":" String ";" ;
JoinStatement       = ">" Identifier [ "[" { Modifier } "]" ] ";" ;
LeaveStatement      = "leave" Identifier ";" ;
StackStatement      = "call" Identifier [ "(" { Expression } ")" ] ";" | "return" ";" ;

CinematicStatement  = "fade_screen" Expression Expression [ "wait" ] ";"
                    | "fade" Identifier Expression Expression [ "wait" ] ";"
                    | "move" Identifier Position Expression [ "wait" ] ";"
                    | "shake" Expression [ "wait" ] ";"
                    | "delay" Expression ";"
                    | "expression" Identifier Identifier ";" ;

UiStatement         = "ui" ("show" | "hide") ";"
                    | "ui" "activate" Identifier ";"
                    | "ui" "deactivate" Identifier ";"
                    | "ui" "load" Identifier String Expression ";"
                    | "ui" "unload" Identifier ";"
                    | "ui" "set" Identifier String Expression ";" ;

PlayTimelineStatement = "play" "timeline" Identifier [ "wait" ] ";" ;
EmitStatement         = "emit" String ";" ;
WaitEventStatement    = "wait" "event" String ";" ;

Modifier            = Identifier "=" ( Expression | "(" Expression "?" Expression ":" Expression ")" ) ;

ChoiceBlock         = "choice" "{" { ChoiceOption } "}" [ ";" ] ;
ChoiceOption        = String [ "[" { Modifier } "]" ] "->" Identifier [ "if" Expression ] ";" ;

JumpStatement       = "jump" Identifier ";" ;
IfStatement         = "if" "(" Expression ")" "{" { SceneStatement } "}" [ ";" ] ;
LocalDecl           = "local" Identifier "=" Expression ";" ;
WaitStatement       = "wait" ("move" | "fade" | "all" | "sfx" | "timeline") ";" ;

Expression          = LogicalExpr ;
LogicalExpr         = ComparisonExpr { ("and" | "or") ComparisonExpr } ;
ComparisonExpr      = AddExpr [ CompareOp AddExpr ] ;
AddExpr             = MulExpr { ("+" | "-") MulExpr } ;
MulExpr             = UnaryExpr { ("*" | "/") UnaryExpr } ;
UnaryExpr           = [ "-" | "!" ] Primary ;
Primary             = Number | String | Identifier | Boolean | "(" Expression ")" ;

CompareOp           = "==" | "!=" | "<" | ">" | "<=" | ">=" ;
Position            = "left" | "right" | "center" | Number ;
TimeValue           = Number [ "ms" | "s" ] ;
Number              = Digit { Digit } [ "." Digit { Digit } ] [ "ms" | "s" ] ;
String              = "\"" { AnyChar } "\"" ;
AssetId             = Identifier | "{" Identifier "}" | String ;
Boolean             = "true" | "false" ;
Identifier          = Letter { Letter | Digit | "_" } ;
```

---

## 🎭 Detailed Syntax

### 1. Project Architecture (`project.json`)
BitEngine uses a centralized `project.json` for all configurations, UI layout definitions, and folder structures. The engine automatically discovers assets (images, music, sfx, fonts) from the specified `assets` subdirectories.

```json
{
    "directories": {
        "assets": "assets/",
        "entities": "assets/entities/",
        "ui": "assets/ui/",
        "sprites": "assets/sprites/"
    },
    "runtime": {
        "scripts": ["scripts/main.bitscript"],
        "start_scene": "init",
        "strict_assets": true
    },
    "ui_layouts": {
        "dialog_ui": {
            "path": "ui_default.json",
            "layer": 10,
            "active": false,
            "visible": true
        }
    }
}
```

### 2. Variables & Constraints
Variables are global state registers. They support optional min/max constraints, and also strings.
```bitscript
var gold = 100 { min = 0; max = 9999; };
var visited_castle = false;
var current_bg = "sky_bg";

// Local string support
local temp_name = "Player";
```

### 3. Entities, Sprites & Aliases
Entities are defined as individual `.json` files in the `assets/entities/` directory. They map character IDs to names, poses, and shortcuts. The filename becomes the entity's ID (e.g., `akira.json` -> ID: `akira`).

- **`sprites`**: Maps expressions to textures. Paths are automatically prefixed with the `sprites` directory from `project.json`.
- **`aliases`**: Shortcuts that set multiple modifiers (like `sprite`, `pos`, `shake`) when used in dialogue.

```json
{
    "name": "Akira",
    "default_pos_x": 0.8,
    "sprites": {
        "idle": { "path": "akira_idle.png", "frames": 2, "speed": 4.0 },
        "angry_sp": { "path": "akira_angry.png" }
    },
    "aliases": {
        "angry": {
            "sprite": "angry_sp",
            "pos": "center",
            "shake": true
        }
    }
}
```

### 4. Rich Text Tags
BitScript dialogue supports inline formatting tags parsed by the **Neural Typewriter**.

| Tag | Example | Description |
| :--- | :--- | :--- |
| `[color=...]` | `[color=RED]` or `[color=#FF0000]` | Changes text color. |
| `[speed=...]` | `[speed=0.5]` | Multiplier for reveal speed. |
| `[wait=...]` | `[wait=1.0]` | Pauses reveal for X seconds. |
| `[shake]` | `[shake]Intensity![/shake]` | Procedural positional jitter. |
| `[wave]` | `[wave]Ethereal...[/wave]` | Procedural sine-wave offset. |
| `[font=...]` | `[font=cursive]` | Switches font to a registered ID. |

---

## 📊 System Variables & Data Binding
The engine exposes internal state through **System Variables** prefixed with `var.`. These can be used in expressions or UI bindings.

| Variable | Type | Description |
| :--- | :--- | :--- |
| `var.entity_name` | String | Name of the current speaker. |
| `var.entity_id` | String | ID of the current speaker. |
| `var.dialog` | String | The fully interpolated text of the current dialogue. |
| `var.is_revealing` | Boolean | `true` if text is still typing. |
| `var.is_waiting_input`| Boolean | `true` if text finished and waiting for click. |
| `var.ui_visible` | Boolean | `true` unless `ui hide` was called. |
| `var.choices_visible` | Boolean | `true` if a choice panel is active. |
| `var.choices` | Array | Array of objects: `{"text": "...", "index": N}`. |

---

### 🎨 UI Asset Management
UI layouts are registered in `project.json` under `ui_layouts`. You define the UI's ID, its file path relative to the `ui` directory, layer order, and its initial state.

```json
    "ui_layouts": {
        "hud": {
            "path": "hud.json",
            "layer": 10,
            "active": true,
            "visible": true
        }
    }
```

---

## 🎭 UI & Cinematic Commands

### 1. UI Lifecycle
The `ui` namespace unifies all interface commands.

| Command | Arguments | Description |
| :--- | :--- | :--- |
| **`ui activate`** | `ui_id` | Activates a registered UI. |
| **`ui deactivate`** | `ui_id` | Suspends a UI. It stops rendering, reacting to input, and updating data. |
| **`ui unload`** | `ui_id` | Completely removes a UI from memory. |
| **`ui set`** | `ui_id.elem_id prop val` | Modifies a property of a specific UI element. |

**Example:**
```bitscript
ui activate hud;
ui set hud.label_gold "content" "{gold}";
wait 2s;
ui deactivate hud;
```

---

## 🎞️ Events & Timelines
- **`event`**: Global named sub-routines. They do not have a call stack but can be triggered via `emit`.
- **`timeline`**: Time-precise animation sequences. Can be played as background tasks or blocking "wait" events.

```bitscript
event on_death {
    fade_screen 1, 0.5s wait;
    narration: "Game Over";
    halt;
}

timeline intro_pan {
    0ms:   { bg sky; fade_screen 0 2s; }
    1500ms: { move akira center 1s; }
    2500ms: { akira: "I've arrived."; }
}

scene start {
    play timeline intro_pan wait;
}
```

---

## 🔍 Static Analyzer (v0.2)
The compiler includes a proactive analyzer that checks:
- **Referential Integrity**: All `jump`, `call`, and `choice` targets must exist.
- **Variable Registry**: Every `var` used must be declared (except `__tmp` variables).
- **Arithmetic Safety**: Literal division by zero is flagged as an error.
- **Asset Integrity**: Backgrounds, BGM, and SFX IDs must be registered in the `assets` block.
- **UI Logic**: `ui_unload` for non-loaded layouts is flagged as a warning.
