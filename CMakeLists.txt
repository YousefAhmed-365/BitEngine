cmake_minimum_required(VERSION 3.16)
project(App)

# Require C++17
set(CMAKE_CXX_STANDARD 17)

# Find Raylib (make sure it's installed via your package manager)
find_package(raylib REQUIRED)

# Include src directory
include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/src/headers)

# Add executable from src
add_executable(${PROJECT_NAME} src/main.cpp src/BitEngine.cpp src/RayDialog.cpp)

# Link Raylib
target_link_libraries(${PROJECT_NAME} raylib)

# Linux extra libs
if(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} m pthread dl)
endif()