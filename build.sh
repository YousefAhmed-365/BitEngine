#!/bin/bash
# Build and run the project with Unified Resource Management

# Set project root and build directory
ROOT_DIR=$(pwd)
BUILD_DIR="build"

# Create build dir if it doesn't exist
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# 1. Modular Resource Linking (Surprise!)
# Instead of copying, we create symlinks to ensure a single source of truth.
# This prevents outdated assets in the build folder.
ln -sfT ../res res
ln -sfT ../assets assets
mkdir -p ../save
ln -sfT ../save save

# 2. Run CMake
cmake -DCMAKE_BUILD_TYPE=Debug ..

# 3. Build the project
cmake --build .

# 4. Run the executable
if [ -f "./BitEngine" ]; then
    ./BitEngine
else
    echo "Build failed: BitEngine executable not found."
fi