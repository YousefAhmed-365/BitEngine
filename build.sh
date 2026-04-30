#!/bin/bash

# Configuration
BUILD_DIR="build"
SAVE_DIR="save"

# 1. Create structure
mkdir -p "$BUILD_DIR"
mkdir -p "$SAVE_DIR"

# 2. Setup Persistent Symlinks (Unified Resource Management)
# This ensures build/res and build/assets always point to project root
[ ! -L "$BUILD_DIR/res" ] && ln -s "$(pwd)/res" "$BUILD_DIR/res"
[ ! -L "$BUILD_DIR/assets" ] && ln -s "$(pwd)/assets" "$BUILD_DIR/assets"
[ ! -L "$BUILD_DIR/save" ] && ln -s "$(pwd)/save" "$BUILD_DIR/save"

# 3. Build Project
cd "$BUILD_DIR"
cmake ..
make -j$(nproc)

# 4. Check if build was successful
if [ $? -eq 0 ]; then
    echo "--- Build Successful! ---"
    echo "Executable: build/BitEngine"
    echo "Compiler: build/BitTool"
    
    # Run the Engine (It will check for data.bin or res/configs.json)
    ./BitEngine
else
    echo "Build FAILED."
    exit 1
fi