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

project(ZibraVDBForHoudini)

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

if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    set(ZIB_NETWORK_REQUEST_BACKEND "WINHTTP")
elseif("${ZIB_TARGET_OS_NAME}" STREQUAL "LINUX")
    set(ZIB_NETWORK_REQUEST_BACKEND "CURL")
elseif("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    set(ZIB_NETWORK_REQUEST_BACKEND "CURL")
else()
    message(FATAL_ERROR "Unsupported target OS: ${ZIB_TARGET_OS_NAME}")
endif()
cmake_print_variables(ZIB_NETWORK_REQUEST_BACKEND)

set(HeaderFiles
    src/PrecompiledHeader.h
    src/Globals.h
    src/bridge/networking/NetworkRequest.h
    src/bridge/LibraryUtils.h
    src/bridge/UpdateCheck.h
    src/ROP/CompressorManager/CompressorManager.h
    src/SOP/DecompressorManager/DecompressorManager.h
    src/licensing/HoudiniLicenseManager.h
    src/licensing/TrialManager.h
    src/ROP/ROP_ZibraVDBCompressor.h
    src/SOP/SOP_ZibraVDBDecompressor.h
    src/ui/MessageBox.h
    src/ui/PluginManagementWindow.h
    src/utils/Helpers.h
    src/utils/GAAttributesDump.h
)

set(SourceFiles
    src/main.cpp
    src/bridge/networking/NetworkRequest.cpp
    src/bridge/LibraryUtils.cpp
    src/bridge/UpdateCheck.cpp
    src/ROP/CompressorManager/CompressorManager.cpp
    src/SOP/DecompressorManager/DecompressorManager.cpp
    src/licensing/HoudiniLicenseManager.cpp
    src/licensing/TrialManager.cpp
    src/ROP/ROP_ZibraVDBCompressor.cpp
    src/SOP/SOP_ZibraVDBDecompressor.cpp
    src/ui/MessageBox.cpp
    src/ui/PluginManagementWindow.cpp
    src/utils/Helpers.cpp
    src/utils/GAAttributesDump.cpp
)

add_library(ZibraVDBForHoudini SHARED ${HeaderFiles} ${SourceFiles})
target_include_directories(ZibraVDBForHoudini
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/Include)
target_precompile_headers(ZibraVDBForHoudini PRIVATE src/PrecompiledHeader.h)
target_compile_definitions(ZibraVDBForHoudini PRIVATE 
    ZIB_NETWORK_REQUEST_BACKEND_${ZIB_NETWORK_REQUEST_BACKEND}=1
    ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1)

target_include_directories(ZibraVDBForHoudini PRIVATE SDK/include)
target_include_directories(ZibraVDBForHoudini PRIVATE "${HOUDINI_INCLUDE_DIR}")

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
    set(OUTPUT_PATH ${CMAKE_MATCH_1})
endif()
cmake_print_variables(OUTPUT_PATH)
set(HOUDINI_INSTDIR "${OUTPUT_PATH}/dso")
cmake_print_variables(HOUDINI_INSTDIR)

if(${LABS_BUILD})
    # Icons and HDA are handled separaetely in Labs
    add_custom_command(TARGET ZibraVDBForHoudini POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/config/Applications/
        ${OUTPUT_PATH}/config/Applications/
    )
    # Labs uses separate file for copyright notices, skip copying 
else()
    add_custom_command(TARGET ZibraVDBForHoudini POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${OUTPUT_PATH}
    )

    add_custom_command(TARGET ZibraVDBForHoudini POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/external/ThirdPartyCopyrightNotices.txt
        ${OUTPUT_PATH}/dso/ZibraVDBForHoudiniThirdPartyCopyrightNotices.txt
    )
endif()

list(APPEND CMAKE_PREFIX_PATH "${HOUDINI_PATH}/toolkit/cmake")
find_package(Houdini REQUIRED)
target_link_libraries(ZibraVDBForHoudini PRIVATE Houdini)

if (NOT "${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    target_link_libraries(ZibraVDBForHoudini PRIVATE Houdini::Dep::openvdb_sesi)
endif()

if(${LABS_BUILD})
    set_target_properties(ZibraVDBForHoudini PROPERTIES PREFIX "")
    install(TARGETS ZibraVDBForHoudini DESTINATION ${HOUDINI_INSTDIR})
else()
    houdini_configure_target(ZibraVDBForHoudini INSTDIR ${HOUDINI_INSTDIR})
endif()

if(MSVC)
    set_target_properties(ZibraVDBForHoudini PROPERTIES
        VS_DEBUGGER_COMMAND "${HOUDINI_PATH}/bin/houdini${CMAKE_EXECUTABLE_SUFFIX}"
        VS_DEBUGGER_COMMAND_ARGUMENTS "${CMAKE_CURRENT_SOURCE_DIR}/HIP/HDAs.hip"
        VS_DEBUGGER_ENVIRONMENT "HOUDINI_OTLSCAN_PATH=${CMAKE_CURRENT_SOURCE_DIR}/assets/otls\nHOUDINI_TEXT_CONSOLE=1"
    )
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT ZibraVDBForHoudini)
endif()
