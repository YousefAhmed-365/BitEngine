cmake_minimum_required(VERSION 3.16)
project(BitEngine VERSION 1.0.0 LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set build type to Debug by default if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Setting build type to 'Debug' as none was specified.")
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Find Raylib
find_package(raylib REQUIRED)

# Common Core Sources
set(CORE_SOURCES
    src/BitEngine.cpp
)

# --- BitEngine (The Game) ---
add_executable(BitEngine src/main.cpp src/BitDialog.cpp ${CORE_SOURCES})
target_include_directories(BitEngine PRIVATE src src/headers)
target_link_libraries(BitEngine PRIVATE raylib)

# --- BitTool (The Compiler) ---
add_executable(BitTool src/BitTool.cpp ${CORE_SOURCES})
target_include_directories(BitTool PRIVATE src src/headers)
target_link_libraries(BitTool PRIVATE raylib) # Still needs Raylib for FileExists etc.

# Compiler flags
set(WARN_FLAGS -Wall -Wextra -Wpedantic)
target_compile_options(BitEngine PRIVATE ${WARN_FLAGS})
target_compile_options(BitTool PRIVATE ${WARN_FLAGS})

# Optimization Flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /GL")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /Ob0 /RTC1")
else()
    # High-performance optimizations for Release
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
endif()

# Linux extra libs
if(UNIX AND NOT APPLE)
    target_link_libraries(BitEngine PRIVATE m pthread dl)
    target_link_libraries(BitTool PRIVATE m pthread dl)
endif()

# Set output directory for convenience
set_target_properties(BitEngine BitTool PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")