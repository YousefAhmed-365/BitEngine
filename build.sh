#!/bin/bash
# Build and run the project

# Set project build directory
BUILD_DIR="build"

# Create build dir if it doesn't exist
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Run CMake to generate build system
cmake ..

# Build the project
cmake --build .

# Run the executable
./App