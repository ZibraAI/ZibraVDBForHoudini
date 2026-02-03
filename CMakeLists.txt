cmake_minimum_required(VERSION 3.25)

include_guard(GLOBAL)

include(CMakePrintHelpers)

# VFX Reference Platform Specific Settings, see See https://vfxplatform.com/
# CY2025
# macOS Minimum Deployment Target = 12.0
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum OS X deployment version")
# C++ Standard = 17
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ Standard")

# C++ settings
# Force it to be as close to C++ standard as possible
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "C++ Standard Required")
set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "C++ Standard Extensions")
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive- /Zc:__cplusplus" CACHE STRING "C++ Compiler Flags")
endif()

# Houdini specific settings
# Set MSVC runtime library to always be MultiThreadedDLL
# So any configuration will use the same runtime library as Houdini
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

option(LABS_BUILD "Build for Labs. Disables certain build system quirks." OFF)
cmake_print_variables(LABS_BUILD)

project(ZibraVDBForHoudini VERSION 1.3.0.0)

set(ZIBRAVDB_PLUGIN_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(ZIBRAVDB_PLUGIN_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(ZIBRAVDB_PLUGIN_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(ZIBRAVDB_PLUGIN_VERSION_TWEAK ${PROJECT_VERSION_TWEAK})

# Target OS detection
if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
    set(ZIB_TARGET_OS_NAME "WIN" CACHE STRING "Target OS Name")
elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set(ZIB_TARGET_OS_NAME "LINUX" CACHE STRING "Target OS Name")
elseif ("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
    set(ZIB_TARGET_OS_NAME "MAC" CACHE STRING "Target OS Name")
else()
    message(FATAL_ERROR "Unknown target OS: ${CMAKE_SYSTEM}.")
endif()
cmake_print_variables(ZIB_TARGET_OS_NAME)

if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        message(STATUS "MSVC platform toolset version: ${CMAKE_VS_PLATFORM_TOOLSET_VERSION}")
        message(STATUS "MSVC version: ${CMAKE_CXX_COMPILER_VERSION}")
    else()
        message(FATAL_ERROR "Unsupported compiler for Windows: ${CMAKE_CXX_COMPILER_ID}.")
    endif()

elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "LINUX")

    set(CMAKE_INSTALL_RPATH "${INSTALL_RPATH}")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(STATUS "GCC version: ${CMAKE_CXX_COMPILER_VERSION}")
    else()
        message(FATAL_ERROR "Unsupported compiler for Linux: ${CMAKE_CXX_COMPILER_ID}.")
    endif()

elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")

    set(CMAKE_INSTALL_RPATH "${INSTALL_RPATH}")
    if (NOT {$LABS_BUILD})
        # Do not bake in absolute path in labs build
        set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        message(STATUS "AppleClang version: ${CMAKE_CXX_COMPILER_VERSION}")
    else()
        message(FATAL_ERROR "Unsupported compiler for macOS: ${CMAKE_CXX_COMPILER_ID}.")
    endif()

else()
    message(FATAL_ERROR "Unknown target OS: ${ZIB_TARGET_OS_NAME}.")
endif()

set(HeaderFiles
    src/PrecompiledHeader.h
    src/ROP/CompressorManager/CompressorManager.h
    src/ROP/ROP_ZibraVDBCompressor.h
    src/SOP/SOP_ZibraVDBDecompressor.h
    src/LOP/LOP_ZibraVDBImport.h
    src/LOP/ZibraVDBOutputProcessor.h
)

set(SourceFiles
    src/main.cpp
    src/ROP/CompressorManager/CompressorManager.cpp
    src/ROP/ROP_ZibraVDBCompressor.cpp
    src/SOP/SOP_ZibraVDBDecompressor.cpp
    src/LOP/LOP_ZibraVDBImport.cpp
    src/LOP/ZibraVDBOutputProcessor.cpp
)

add_library(ZibraVDBForHoudini SHARED ${HeaderFiles} ${SourceFiles})
target_include_directories(ZibraVDBForHoudini
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/Include)
target_precompile_headers(ZibraVDBForHoudini PRIVATE src/PrecompiledHeader.h)
target_compile_definitions(ZibraVDBForHoudini PRIVATE
    ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1)

if(DEFINED ENV{ZIBRA_HOUDINI_PATH})
    message(STATUS "ENV{ZIBRA_HOUDINI_PATH}: $ENV{ZIBRA_HOUDINI_PATH}")
    set(HOUDINI_PATH "$ENV{ZIBRA_HOUDINI_PATH}")
elseif(DEFINED ENV{HFS})
    message(STATUS "ENV{HFS}: $ENV{HFS}")
    set(HOUDINI_PATH "$ENV{HFS}")
else()
    message(FATAL_ERROR "ZIBRA_HOUDINI_PATH and HFS environment variables are not set. Can't find Houdini installation.")
endif()
cmake_print_variables(HOUDINI_PATH)

if(DEFINED ZIBRAVDB_OUTPUT_PATH)
    message(STATUS "Using specified install directory")
    set(OUTPUT_PATH ${ZIBRAVDB_OUTPUT_PATH})
else()
    # Manually finding install dir so houdini_configure_target
    # doesn't need to check out license when querying it with hython
    message(STATUS "Querying hconfig for HOUDINI_USER_PREF_DIR")
    if(UNIX)
        execute_process(COMMAND bash -c "cd ${HOUDINI_PATH}; source ./houdini_setup; ${HOUDINI_PATH}/bin/hconfig${CMAKE_HOST_EXECUTABLE_SUFFIX}"
            OUTPUT_VARIABLE HOUDINI_ENV_VARS
        )
    else()
        execute_process(COMMAND "${HOUDINI_PATH}/bin/hconfig${CMAKE_HOST_EXECUTABLE_SUFFIX}"
            OUTPUT_VARIABLE HOUDINI_ENV_VARS
        )
    endif()
    string(REGEX MATCH "HOUDINI_USER_PREF_DIR := '([^\n]*)'" _ ${HOUDINI_ENV_VARS})

    if(${LABS_BUILD})
        set(OUTPUT_PATH "${CMAKE_MATCH_1}")
    else()
        set(OUTPUT_PATH "${CMAKE_MATCH_1}/packages")
    endif()
endif()
cmake_print_variables(OUTPUT_PATH)
list(APPEND CMAKE_PREFIX_PATH "${HOUDINI_PATH}/toolkit/cmake")
find_package(Houdini REQUIRED)

set(DETECTED_HOUDINI_VERSION "${Houdini_VERSION_MAJOR}.${Houdini_VERSION_MINOR}")
cmake_print_variables(DETECTED_HOUDINI_VERSION)

add_subdirectory(common)
setup_common_houdini_dependencies(ZibraVDBForHoudini)
add_subdirectory(AssetResolver)

if(${LABS_BUILD})
    set_target_properties(ZibraVDBForHoudini PROPERTIES PREFIX "")
    install(TARGETS ZibraVDBForHoudini DESTINATION "${OUTPUT_PATH}/dso")
else()
    houdini_configure_target(ZibraVDBForHoudini INSTDIR "${CMAKE_CURRENT_BINARY_DIR}/HOUDINI_INSTDIR")
endif()

if(${LABS_BUILD})
    # In Labs build we use simple post-build flow
    # Only copy files that are not handled in special way by Labs
    # Like Icons, HDAs and copyright notices
    add_custom_target(ZibraVDBForHoudiniPostBuild ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/config/Applications/
        ${OUTPUT_PATH}/config/Applications/
    )
    add_dependencies(ZibraVDBForHoudiniPostBuild ZibraVDBForHoudini)
else()
    add_custom_target(ZibraVDBForHoudiniPostBuild ALL
        COMMAND ${CMAKE_COMMAND} -E env NO_COLOR=1 pwsh "${CMAKE_CURRENT_SOURCE_DIR}/buildSteps/post-build.ps1" "${HOUDINI_PATH}" "${CMAKE_PROJECT_VERSION}" "$<TARGET_FILE:ZibraVDBForHoudini>" "$<TARGET_FILE:ZibraVDBResolver>" "${ZIBRAVDB_ASSET_RESOLVER_PLUGIN_CONFIG}" "${OUTPUT_PATH}"
    )
    target_sources(ZibraVDBForHoudiniPostBuild PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/buildSteps/post-build.ps1
    )
    add_dependencies(ZibraVDBForHoudiniPostBuild ZibraVDBForHoudini ZibraVDBResolver)
endif()

if(MSVC)
    set_target_properties(ZibraVDBForHoudiniPostBuild PROPERTIES
        VS_DEBUGGER_COMMAND "${HOUDINI_PATH}/bin/houdini${CMAKE_EXECUTABLE_SUFFIX}"
        VS_DEBUGGER_COMMAND_ARGUMENTS "${CMAKE_CURRENT_SOURCE_DIR}/HIP/zibravdb_example.hip"
        VS_DEBUGGER_ENVIRONMENT "HOUDINI_TEXT_CONSOLE=1"
    )
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT ZibraVDBForHoudiniPostBuild)
endif()
