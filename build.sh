#!/bin/bash

# Configuration
BUILD_DIR="build"
SAVE_DIR="save"

# Default to Debug if no argument is given
BUILD_TYPE="Debug"

# Parse arguments
if [ "$1" == "release" ] || [ "$1" == "Release" ]; then
    BUILD_TYPE="Release"
elif [ "$1" == "debug" ] || [ "$1" == "Debug" ]; then
    BUILD_TYPE="Debug"
elif [ "$1" != "" ]; then
    echo "Usage: ./build.sh [debug|release]"
    exit 1
fi

echo "--- Building BitEngine in $BUILD_TYPE mode ---"

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
cmake -DCMAKE_BUILD_TYPE=$BUILD_TYPE ..
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