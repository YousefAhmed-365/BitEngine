# 🌌 BitEngine v0.2
**The High-Performance, Bytecode-Driven Narrative Engine**

BitEngine is a modern, modular visual novel and narrative adventure engine built with C++17 and Raylib. It features a custom virtual machine, a data-driven UI system, and a robust cinematic pipeline.

---

## 🚀 Key Features

- **Bytecode VM**: Executes strictly-typed `.bitc` binaries for O(1) instruction dispatch and native performance.
- **BitScript Language**: A declarative scripting language designed for narrative branching, complex logic, and cinematic control.
- **Neural Typewriter**: A procedural rich-text rendering system with per-character formatting (shake, wave, speed, color).
- **Decoupled UI**: Interaction logic is completely independent of narrative processing, ensuring zero-latency responsiveness.
- **Static Analysis**: Proactive compiler validation for referential integrity, variable auditing, and asset consistency.
- **Cinematic Pipeline**: Manual alpha interpolation for screens and entities, procedural breathing effects, and dynamic shadows.
- **Unified Debugging**: F3 overlay for real-time VM inspection, variable watching, and bytecode tracing.

---

## 🎮 Controls & Shortcuts

| Key | Action |
| :--- | :--- |
| **SPACE / ENTER / CLICK** | Advance dialogue / Select option / Skip reveal. |
| **H** | Toggle scrollable Message History. |
| **TAB** | Toggle Cinematic Mode (Hide UI). |
| **A** | Toggle Auto-Play mode. |
| **F5** | Quick Save to Slot 1. |
| **F9** | Quick Load from Slot 1. |
| **F3** | Toggle Unified Debug Overlay. |
| **ESC** | Open System Menu / Close History. |

---

## 🛠️ Getting Started

### 1. Build Requirements
- **Compiler**: C++17 compatible (GCC 9+, Clang 10+, MSVC 2019+).
- **Libraries**: Raylib 4.5+, `nlohmann/json`.
- **System**: Linux, Windows, or macOS.

### 2. Compilation
```bash
./build.sh debug    # Build with debug symbols and instrumentation
./build.sh release  # Build optimized production binary
```

### 3. Usage
```bash
# Compile a BitScript project
./BitEngine --compile scripts/main.bitscript scripts/main.bitc

# Run the project
./BitEngine --run scripts/main.bitc
```

---

## 📂 Documentation
- [🧩 BitScript Specification](docs/BitScript_doc.md): Language syntax and usage.
- [⚙️ Engine Internals](docs/Engine_Internals.md): Technical VM specs and internal architecture.
- [📁 Features List](features.txt): Comprehensive breakdown of all v0.2 capabilities.

---

## 📜 License
MIT License - Copyright (c) 2026 Yousef Ahmed.