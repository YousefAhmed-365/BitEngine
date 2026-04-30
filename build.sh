#!/bin/bash
# Build and run the project

# Set project build directory
BUILD_DIR="build"

# Create build dir if it doesn't exist
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Run CMake to generate build system (explicitly Debug)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Build the project
cmake --build .

# Run the executable
if [ -f "./BitEngine" ]; then
    ./BitEngine
else
    echo "Build failed: BitEngine executable not found."
fi