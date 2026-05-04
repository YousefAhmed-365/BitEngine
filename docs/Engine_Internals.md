# ⚙️ BitEngine v0.2 Internals & Technical Reference

This document provides a deep dive into the internal mechanics of the BitEngine, covering the Virtual Machine, Rendering Pipeline, and Static Analysis systems.

---

## 🧠 Virtual Machine (VM)

The BitEngine VM is a register-based virtual machine that executes strictly-typed bytecode instructions (`.bitc`).

### 1. Instruction Set (Opcodes)

| Opcode | Arguments | Description |
| :--- | :--- | :--- |
| **`SAY`** | `entity`, `content` | Displays dialogue. Triggers "Join" if character is inactive. |
| **`CHOICE`** | `text`, `target` | Registers an interactive branch. |
| **`GOTO`** | `label` | Jumps to a specific instruction index. |
| **`IF` / `IF_REF`** | `var`, `op`, `val`, `label` | Conditional jump. `IF_REF` compares two variables. |
| **`SET` / `SET_REF`** | `var`, `val` | Assigns a value to a global variable. |
| **`ADD`, `SUB`, `MUL`, `DIV`** | `var`, `val` | Arithmetic operations (with `_REF` variants). |
| **`CALL`** | `label`, `args...` | Pushes current PC and local scope; jumps to label. |
| **`RETURN`** | (none) | Pops PC and scope, returning to caller. |
| **`SET_LOCAL`** | `var`, `val` | Sets a variable in the current scene's local scope. |
| **`WAIT_INPUT`** | (none) | Pauses VM until user advances dialogue. |
| **`WAIT_ACTION`**| `type` | Pauses VM until visual/audio tasks (`fade`, `move`, `all`) finish. |
| **`WAIT_EVENT`** | `event_id` | Pauses VM until an external event is received. |
| **`EMIT`** | `event_id` | Triggers a global event block. |
| **`PLAY_TIMELINE`**| `id`, `blocking` | Starts a timeline sequence. |
| **`UI_LOAD`** | `name`, `path`, `layer` | Requests the renderer to load a layout. |
| **`UI_SET`** | `id`, `prop`, `val` | Modifies a UI element property. |
| **`HALT`** | (none) | Immediately terminates VM execution. |

### 2. Narrative Stack & Scoping
- **Global Scope**: Any variable declared with `var`. Persists for the entire game session.
- **Local Scope**: Created during a `CALL` instruction. Cleared on `RETURN`. Local variables shadow globals of the same name.
- **Save State**: The VM saves the current **Program Counter (PC)**, the entire variable map, the call stack (return addresses), and active entity positions.

---

## 🎨 Rendering & UI System

The UI is entirely data-driven via `ui_*.json` (layouts) and `style_*.json` (visuals).

### 1. UI Style Properties
The following properties can be used in `UIStyleBlock` definitions:

- **Panel Visuals**: `bgColor`, `borderColor`, `borderThick`, `roundness`, `opacity`.
- **Textures**: `texture` (path), `nineSlice` (bool), `sliceLeft/Right/Top/Bottom` (px margins), `tint` (color).
- **Text**: `textColor`, `fontSize`, `lineSpacing`, `fontPath`.
- **Cursors**: `cursorShape` (`triangle`, `dot`, `bar`), `cursorColor`, `cursorSize`, `cursorAnimSpeed`.
- **Choices**: `optionColor`, `optionHover`, `optionPremium`, `optionFontSize`, `optionHeight`, `optionGap`.
- **Entities**: `entityScale`, `floatAmplitude` (breathing intensity), `floatSpeed`.

### 2. Standard Roles
The engine binds logic to UI elements based on their `role`:
- `dialog_text`: Receives typewriter content.
- `name_label`: Receives current speaker name.
- `choice_list`: Container for interactive options.
- `dialog_cursor`: Indicator shown when waiting for input.
- `screen_fade`: Full-screen rectangle used for `fade_screen`.
- `mouse_cursor`: Custom skin for the system pointer.
- `toast`: Notification overlay for ephemeral messages.

---

## 🔐 Security & Persistence

### 1. XOR Encryption
BitEngine narrative files (`.bitc`) and save files (`.bin`) are encrypted using a rotating XOR key:
- **Static Key**: `BITENGINE_SECRET_KEY_2026`
- **Algorithm**: `data[i] ^= KEY[i % KEY_LENGTH]`
- **Impact**: All narrative content is obfuscated from data miners, and save files are protected against manual tamper-editing.

### 2. Save Structure (Binary/JSON)
The save file is a serialized JSON object, then XOR-encrypted. Key fields:
- `current_pc`: The instruction index the VM was on.
- `variables`: A flat key-value map of all global game state.
- `active_entities`: A map of character IDs to their screen state (X position, Alpha, Expression).
- `active_bg/bgm`: The state of the environment.
- `meta`: Metadata for the Save/Load menu (timestamp, speaker name, node summary).

---

## 📡 Messaging & System Events
The engine supports an internal messaging queue for non-narrative notifications.
- **Toast Notifications**: Ephemeral, non-blocking UI overlays.
  - Slot 1 Quick-Save (F5) -> `m_toastMsg = "QUICK SAVE..."`
  - Slot 1 Quick-Load (F9) -> `m_toastMsg = "RELOADING..."`
- **Variable Mirroring**: Every integer variable `gold` is mirrored as a string `var.gold` for the UI Data Store, allowing direct `{var.gold}` bindings.

---

## 🎲 Advanced Logic Ops
- **`random`**: Available via event blocks. Computes `lo + (rand() % (hi - lo + 1))`.
- **`clear`**: Instantly wipes all active characters from the scene.
- **`hide`**: Disables rendering for a specific character while keeping their state in memory.
- **`jump`**: An internal event-level jump that allows re-routing narrative flow from within an event or timeline.

---

## 🛠️ Debug Instrumentation (F3 Overlay)

The **Unified Debug Overlay** provides real-time telemetry into the engine state:

1.  **VM Inspector**: Monitors PC, speaker ID, and input lockout timers.
2.  **Variable Watcher**: Live view of all global variables (including constraints).
3.  **Event Trace**: A 50-entry log of the most recent variable changes and logic branches.
4.  **Local Scope & Stack**: Visualizes the current call depth and active local variables.
5.  **Bytecode Inspector**: A scrollable source-view of the compiled narrative, highlighting the active instruction.
    - **Green Highlight**: Next instruction to be executed.
    - **Yellow Highlight**: Most recently executed instruction.
6.  **Asset Audit**: Lists loaded textures, fonts, and active audio streams.
