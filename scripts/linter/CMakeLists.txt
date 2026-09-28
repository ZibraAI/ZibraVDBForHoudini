cmake_minimum_required(VERSION 3.25)

set(CMAKE_CONFIGURATION_TYPES Release)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# clang-tidy can't read PCH made by MSVC
set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)

project(ZibraVDBForHoudiniLint LANGUAGES NONE)

include(FetchContent)
FetchContent_Declare(ClangTools
    GIT_REPOSITORY https://github.com/ZibraAI/ClangTools.git
    GIT_TAG v22.1.8)
FetchContent_MakeAvailable(ClangTools)

add_subdirectory(../.. ZibraVDBForHoudini)
