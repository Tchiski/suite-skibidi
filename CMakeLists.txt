cmake_minimum_required(VERSION 3.22)

project(TchiskibidiSuite VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) on macOS
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

# JUCE is downloaded automatically the first time you configure the project.
# If you already have JUCE on disk, pass -DJUCE_DIR=/path/to/JUCE instead.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Shared code for the whole Tchiskibidi suite (look & feel, colours, ...)
set(TCHISKIBIDI_SHARED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Shared)

# --- Plugins of the suite -------------------------------------------------
add_subdirectory(Plugins/Space)
# Next ones: add_subdirectory(Plugins/Distortion), Plugins/Filter, ...
