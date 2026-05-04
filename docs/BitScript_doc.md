# 🧩 BitScript Specification & Documentation
**Version 0.2**

BitScript is a lightweight, high-performance narrative scripting language designed specifically for the **BitEngine VM**. It combines declarative entity management with a powerful expression-native logic system to enable complex branching storytelling with minimal syntax overhead.

---

## 🎮 Language Overview

A BitScript project is composed of several global blocks:
1.  **`config`**: Global engine settings and project metadata.
2.  **`var`**: Global state variables (persistent across saves).
3.  **`entities`**: Definitions for characters, system actors, and moods.
4.  **`assets`**: Registry for backgrounds, music, sfx, and fonts.
5.  **`event`**: Named logic blocks that can be triggered externally or via `emit`.
6.  **`timeline`**: Time-precise sequences of commands for complex cutscenes.
7.  **`scene`**: The core execution nodes containing dialogue and logic.

---

## ⚙️ Core Structure (EBNF)

```ebnf
Program             = { GlobalStatement } ;

GlobalStatement     = ConfigBlock
                    | VariableDecl
                    | EntityBlock
                    | AssetBlock
                    | EventBlock
                    | TimelineBlock
                    | SceneBlock
                    | [ ";" ] ;

ConfigBlock         = "config" "{" { ConfigEntry } "}" ";" ;
ConfigEntry         = Identifier "=" Expression ";" ;

VariableDecl        = "var" Identifier [ "=" Expression ] [ "{" { RangeProp } "}" ] ";" ;
RangeProp           = ("min" | "max") "=" Expression ";" ;

Assignment          = Identifier ( "=" | "+=" | "-=" | "*=" | "/=" ) Expression ";" ;

EntityBlock         = "entities" "{" { Entity } "}" [ ";" ] ;
Entity              = Identifier "{" { EntityProperty } "}" [ ";" ] ;
EntityProperty      = "name" "=" String ";"
                    | "default_pos" "=" Position ";"
                    | SpriteBlock
                    | AliasBlock ;

SpriteBlock         = "sprite" Identifier "{" { SpriteProperty } "}" [ ";" ] ;
SpriteProperty      = "path" "=" String ";"
                    | "frames" "=" Expression ";"
                    | "speed" "=" Expression ";"
                    | "scale" "=" Expression ";" ;

AliasBlock          = "alias" Identifier "{" { AliasProperty } "}" [ ";" ] ;
AliasProperty       = Identifier "=" Expression [ "," ] ;

AssetBlock          = "assets" "{" { AssetTypeBlock } "}" [ ";" ] ;
AssetTypeBlock      = ("bg" | "music" | "sfx" | "fonts") "{" { AssetEntry } "}" ;
AssetEntry          = Identifier "=" String ";" ;

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
                    | WaitEventStatement ;

Dialogue            = Identifier [ "." Identifier ] [ "[" { Modifier } "]" ] ":" String ";" ;
NarrationStatement  = "narration" [ "[" { Modifier } "]" ] ":" String ";" ;
JoinStatement       = ">" Identifier [ "[" { Modifier } "]" ] ";" ;
LeaveStatement      = "leave" Identifier ";" ;
StackStatement      = "call" Identifier [ "(" { Expression } ")" ] ";" | "return" ";" ;

CinematicStatement  = "fade_screen" Expression "," Expression [ "wait" ] ";"
                    | "fade" Identifier "," Expression "," Expression [ "wait" ] ";"
                    | "move" Identifier "," Position "," Expression [ "wait" ] ";"
                    | "shake" Expression [ "wait" ] ";"
                    | "delay" Expression ";"
                    | "expression" Identifier "," Identifier ";"
                    | "play_sfx" Identifier ";" ;

UiStatement         = "ui" ("show" | "hide") ";"
                    | "ui_load" String "," String "," Expression ";"
                    | "ui_unload" String ";"
                    | "ui_set" String "," String "," Expression ";" ;

PlayTimelineStatement = "play" "timeline" ( Identifier | TimelineBlock ) [ "wait" ] ";" ;
EmitStatement         = "emit" String ";" ;
WaitEventStatement    = "wait" "event" String ";" ;

Modifier            = Identifier "=" Expression ;

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
Boolean             = "true" | "false" ;
Identifier          = Letter { Letter | Digit | "_" } ;
```

---

## 🎭 Detailed Syntax

### 1. Config Block
Sets engine-level parameters. 
- `start_node`: The ID of the first scene to execute.
- `mode`: Interaction mode (`typewriter` or `instant`).
- `reveal_speed`: Characters per second (base speed).
- `auto_save`: Enable automatic binary state persistence on every dialogue block.
- `max_slots`: Number of manual save slots available (default 5).
- `enable_floating`: Globally toggle character breathing animations.
- `enable_shadows`: Globally toggle character drop shadows.

```bitscript
config {
    start_node = intro_scene;
    reveal_speed = 45;
    auto_save = true;
    max_slots = 10;
};
```

### 2. Variables & Constraints
Variables are global state registers. They support optional min/max constraints.
```bitscript
var gold = 100 { min = 0; max = 9999; };
var visited_castle = false;
```

### 3. Entities, Sprites & Aliases
Entities define characters. **Aliases** allow mapping a "mood" to multiple properties at once.
- **`sprite`**: Define frame-based animations or static textures.
- **`alias`**: A shortcut that can set `sprite`, `pos`, `alpha`, or other modifiers when used in dialogue.

```bitscript
entities {
    akira {
        name = "Akira";
        default_pos = right;

        sprite idle { path = "assets/akira_idle.png"; frames = 2; speed = 4.0; };
        sprite angry_sp { path = "assets/akira_angry.png"; frames = 1; };

        alias angry {
            sprite = angry_sp,
            pos = center,
            shake = true
        };
    };
};
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

## 🖼️ UI Management & Scoped IDs
BitEngine uses a data-driven UI system. Layouts are defined in JSON and managed via script.

- **`ui_load "name", "path", layer;`**: Mounts a new layout.
- **`ui_unload "name";`**: Unmounts a layout.
- **`ui_set "scoped_id", "property", value;`**: Modifies a specific element.
  - *Scoped ID*: Format `layout_name.element_id` (e.g. `inventory.gold_label`).
  - *Properties*: `visible`, `content`, `opacity`, `x`, `y`, `w`, `h`.

```bitscript
ui_load "hud", "res/ui/hud.json", 10;
ui_set "hud.gold_label", "content", "{gold}G";
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
    0ms:   { bg = sky; fade_screen 0, 2s; }
    1500ms: { move akira, center, 1s; }
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
