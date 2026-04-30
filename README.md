# BitEngine (Work in Progress)

**BitEngine** is a high-performance, data-driven cinematic narrative engine built with C++17 and [Raylib](https://www.raylib.com/). It is designed to cleanly separate story creation from engine compilation, providing a highly modular architecture for visual novels and interactive dialogue experiences.

> [!WARNING]
> **Heavily In Development:** BitEngine is actively being built. Many features are experimental, and the API/configuration formats are subject to change.

## 🚀 Key Features

* **Data-Driven Architecture**: The entire world state, dialogues, entities, and assets are defined in modular JSON files (`configs.json`, `entities.json`, `dialog_intro.json`, etc.).
* **Dual-Binary Deployment**:
  * **BitEngine**: The immersive, hardware-accelerated Raylib game client.
  * **BitTool**: A dedicated asset orchestration CLI that "bakes" your distributed JSON scripts into a single, high-performance binary.
* **Asset Security & Bundling**: BitTool compiles the entire project into an XOR-encrypted `data.bin` file, protecting your un-shipped story files and assets from casual prying.
* **Procedural VFX & Audio**: Supports screen shaking, dynamic vignette drop shadows, positional sprite rendering (left, center, right), and integrated BGM/SFX audio channels.
* **Semantic Asset Registries**: Never hardcode file paths into a scene again. Map `market_bg` to `assets/market_bg.png` once in `assets.json` and cleanly reference it anywhere.
* **Robust Save/Load System**: Built-in state management for player decisions, branching paths, and internal variables. Also outputs XOR-encrypted save slots.

## 🛠️ Building the Project

Ensure you have `CMake` (3.16+) and `Raylib` installed on your system.

### Compiling
Use the provided universal build script:
```bash
# For development (fast compile, debugging symbols)
./build.sh debug

# For production (maximum optimization: -O3, LTO)
./build.sh release
```

### Running the Engine
By default, the engine favors production assets.
1. Engine checks for `build/data.bin` first (Instant binary load).
2. If missing, it falls back to `build/res/configs.json` for hot-reloadable JSON development.

## 📦 Project Structure

* `/src`: C++ Source Code (`BitEngine`, `BitDialog`, `BitTool`).
* `/res`: JSON configuration files for scripts, variables, assets, and entities.
* `/assets`: Media files (sprites, music, sound effects, backgrounds).
* `/build`: Target directory for executables, symlinks, and `data.bin`.

## 📜 Future Roadmap
* Branching dialogue localization (i18n).
* Full UI tokenization and styling overhauls.
* Granular save-file metadata integration.
* Advanced scripting event hooks (camera pans, particle emitters).
