# BitEngine Studio & Narrative Core

**BitEngine** is a lightweight, high-performance narrative engine and visual development suite built with C++17. It is designed to bridge the gap between hard-coded narrative logic and professional-grade visual storytelling.

The project is split into two primary components:
1. **BitEngine Core**: A modular, hardware-agnostic narrative runtime.
2. **BitEngine Studio (BitTool)**: A complete visual IDE for editing dialog graphs, managing global variables, and defining entity registries.

---

## 💎 Key Features

### 🎬 Visual Studio (BitTool)
The **BitEngine Studio** is no longer just a compiler; it's a full-featured visual editor:
- **Infinite Canvas Graph Editor**: Visual node-based workflow for complex branching narratives.
- **Asset Pipeline**: Integrated editors for **Entities** (sprite-mapping, framing, scaling) and **Variables** (global state with min/max constraints).
- **Project Hub**: Manage multiple narrative projects from a single tile-based launcher.
- **Smart Logic**: Real-time validation and automatic persistence to the project's data schema.

### 🧠 Decoupled Core Architecture
The engine's logic layer is entirely independent of the rendering framework:
- **Core Logic**: Handles state, variable manipulation, branching, and binary serialization without any graphic library dependencies.
- **Rich Text Parser**: Support for in-line styling tags: `[color=#hex]`, `[shake]`, `[wave]`, `[speed=2.0]`, and `[wait=1.0]`.
- **Hardware-Agnostic State**: The engine uses a custom `BitColor` system, allowing it to be easily ported to other rendering backends (SDL, SFML, etc.).

### 🎭 Data-Driven Rendering (BitRenderer)
A highly flexible rendering layer driven by a modular **Style System**:
- **Hot-Reloadable Themes**: Define entire UI looks (dialog boxes, choice panels, toast notifications) in `style.json` and watch changes reflect instantly without restarting.
- **Style Manager**: Switch between themes (e.g., `neon_noir`, `minimal`, `vertical_left`) at runtime.
- **Dynamic VFX**: Integrated sprite floating, screen-shakes, and vignette systems.

---

## 🛠️ Building & Running

### Prerequisites
- **Compiler**: C++17 compatible (GCC 9+, Clang 10+, MSVC 2019+)
- **CMake**: 3.16 or higher
- **Raylib**: Standard installation for windowing and graphics.

### Quick Build
Use the provided automation script:
```bash
./build.sh
```

### Modes of Execution
1. **BitEngine**: The game client. It automatically attempts to load XOR-encrypted `data.bin` for production or falls back to raw `/res` files for development.
2. **BitTool**: The Engine Studio. Use this to create new projects or visually edit your scripts.

---

## 📂 Project Structure

- **`/src`**: Modular source code.
  - `BitEngine`: Core logic and narrative state.
  - `BitRenderer`: High-level UI and graphics bridge.
  - `BitEditor`: The logic for the Visual Studio canvas.
- **`/res`**: Project configuration, global variables, and engine settings.
- **`/assets`**: Textures, soundtracks, and sound effects.

---

## ⌨️ Studio Controls
| Action | Hardware |
| :--- | :--- |
| **Pan Canvas** | Right-Click + Drag |
| **Zoom** | Mouse Wheel |
| **Add Node** | Top bar button or `A` key |
| **Select Node** | Left Click |
| **Link Nodes** | Click & Drag from **Circle Pin** to another Node |
| **Hot-Reload Styles** | Press `TAB` in BitEngine |

---

> [!TIP]
> To create your first project, launch **BitTool** and use the bottom-bar creation wizard. It will automatically scaffold the necessary directory structures for you.