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

# ============================================================================
# CORE VM & COMPILATION PIPELINE
# ============================================================================
set(CORE_VM_SOURCES
    "src/Core VM & Compilation/BitLexer.cpp"
    "src/Core VM & Compilation/BitParser.cpp"
    "src/Core VM & Compilation/BitParserStatements.cpp"
    "src/Core VM & Compilation/BitCompiler.cpp"
    "src/Core VM & Compilation/BitVM.cpp"
)

# ============================================================================
# RUNTIME & STATE MANAGEMENT
# ============================================================================
set(RUNTIME_STATE_SOURCES
    "src/Runtime & State/BitRuntime.cpp"
    "src/Runtime & State/BitState.cpp"
    "src/Runtime & State/BitRichText.cpp"
    "src/Runtime & State/BitScriptAnalyzer.cpp"
    "src/Runtime & State/BitScriptInterpreter.cpp"
    "src/Runtime & State/BitDebugger.cpp"
    "src/Runtime & State/BitScheduler.cpp"
)

# ============================================================================
# RENDERING & AUDIO
# ============================================================================
set(RENDER_AUDIO_SOURCES
    "src/Rendering & UI/BitGraphics.cpp"
    "src/Rendering & UI/BitAudio.cpp"
    "src/Rendering & UI/UIRenderer.cpp"
)

# ============================================================================
# UI SYSTEM
# ============================================================================
set(UI_SOURCES
    "src/Rendering & UI/UILayout.cpp"
)

# ============================================================================
# RENDERER INTEGRATION
# ============================================================================
set(RENDERER_SOURCES
    "src/Rendering & UI/BitRenderer.cpp"
    "src/Rendering & UI/BitRendererDraw.cpp"
)

# ============================================================================
# APPLICATION LAYER
# ============================================================================
set(APP_SOURCES
    "src/Application/BitApp.cpp"
    "src/Application/main.cpp"
)

# Combine all sources
set(ALL_SOURCES
    ${CORE_VM_SOURCES}
    ${RUNTIME_STATE_SOURCES}
    ${RENDER_AUDIO_SOURCES}
    ${UI_SOURCES}
    ${RENDERER_SOURCES}
    ${APP_SOURCES}
)

# --- BitEngine (The Game) ---
add_executable(BitEngine ${ALL_SOURCES})
target_include_directories(BitEngine PRIVATE 
    src 
    "src/Core VM & Compilation" 
    "src/Runtime & State" 
    "src/Rendering & UI" 
    "src/Application"
)
target_link_libraries(BitEngine PRIVATE raylib)

# Compiler flags
set(WARN_FLAGS -Wall -Wextra -Wpedantic)
target_compile_options(BitEngine PRIVATE ${WARN_FLAGS})

# Optimization Flags
if(MSVC)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /GL")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /Ob0 /RTC1")
else()
    # Cross-platform friendly optimizations for Release
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
endif()

# Linux extra libs
if(UNIX AND NOT APPLE)
    target_link_libraries(BitEngine PRIVATE m pthread dl)
endif()

# Set output directory for convenience
set_target_properties(BitEngine PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")