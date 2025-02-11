cmake_minimum_required(VERSION 3.25)

include(CMakePrintHelpers)

# Set MSVC runtime library to always be MultiThreadedDLL
# So any configuration will use the same runtime library as Houdini
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

project(ZibraVDBForHoudini)

# Target platform identification
cmake_print_variables(CMAKE_SYSTEM_NAME)
if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(OS_NAME "Win")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    set(OS_NAME "Mac")
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(OS_NAME "Linux")
else()
    message(FATAL_ERROR "Unsupported OS")
endif()
cmake_print_variables(OS_NAME)
string(TOUPPER ${OS_NAME} OS_NAME_CAPITALIZED)

# Using C++17 without vendor extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    cmake_print_variables(CMAKE_VS_PLATFORM_TOOLSET_VERSION)
    # Disable Microsoft extensions and enable correct __cplusplus macro
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive- /Zc:__cplusplus")
else()
    # TODO cross-platform support
endif()

set(HeaderFiles
        src/PrecompiledHeader.h
        src/Globals.h
        src/bridge/networking/NetworkRequest.h
        src/bridge/LibraryUtils.h
        src/bridge/RHIWrapper/RHIWrapper.h
        src/bridge/Licensing/Licensing.h
        src/openvdb/OpenVDBDecoder.h
        src/openvdb/OpenVDBEncoder.h
        src/openvdb/MathHelpers.h
        src/ROP/ROP_ZibraVDBCompressor.h
        src/SOP/SOP_ZibraVDBDecompressor.h
        src/ui/MessageBox.h
        src/utils/GAAttributesDump.h
)

set(SourceFiles
        src/main.cpp
        src/bridge/networking/NetworkRequest.cpp
        src/bridge/LibraryUtils.cpp
        src/bridge/RHIWrapper/RHIWrapper.cpp
        src/bridge/Licensing/Licensing.cpp
        src/openvdb/OpenVDBDecoder.cpp
        src/openvdb/OpenVDBEncoder.cpp
        src/openvdb/MathHelpers.cpp
        src/ROP/ROP_ZibraVDBCompressor.cpp
        src/SOP/SOP_ZibraVDBDecompressor.cpp
        src/ui/MessageBox.cpp
        src/utils/GAAttributesDump.cpp
)

add_library(ZibraVDBForHoudini SHARED ${HeaderFiles} ${SourceFiles})
target_include_directories(ZibraVDBForHoudini
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/Include)
target_precompile_headers(ZibraVDBForHoudini PRIVATE src/PrecompiledHeader.h)
target_compile_definitions(ZibraVDBForHoudini PRIVATE ZIB_PLATFORM_${OS_NAME_CAPITALIZED}=1)

target_include_directories(ZibraVDBForHoudini PRIVATE SDK/include)

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

if (DEFINED H_OUTPUT_INSTDIR)
    message(STATUS "Using specified install directory")
    set(HOUDINI_INSTDIR ${H_OUTPUT_INSTDIR})
else()
    # Manually finding install dir so houdini_configure_target
    # doesn't need to check out license when querying it with hython
    message(STATUS "Querying hconfig for HOUDINI_USER_PREF_DIR")
    execute_process(COMMAND "${HOUDINI_PATH}/bin/hconfig${CMAKE_HOST_EXECUTABLE_SUFFIX}"
                    OUTPUT_VARIABLE HOUDINI_ENV_VARS
    )
    string(REGEX MATCH "HOUDINI_USER_PREF_DIR := '([^\n]*)'" _ ${HOUDINI_ENV_VARS})
    set(HOUDINI_USER_PREF_DIR ${CMAKE_MATCH_1})
    cmake_print_variables(HOUDINI_USER_PREF_DIR)
    set(HOUDINI_INSTDIR "${HOUDINI_USER_PREF_DIR}/dso")
endif()
cmake_print_variables(HOUDINI_INSTDIR)

list(APPEND CMAKE_PREFIX_PATH "${HOUDINI_PATH}/toolkit/cmake")
find_package(Houdini REQUIRED)
target_link_libraries(ZibraVDBForHoudini PRIVATE Houdini Houdini::Dep::openvdb_sesi)

houdini_configure_target(ZibraVDBForHoudini INSTDIR ${HOUDINI_INSTDIR})
install(TARGETS ZibraVDBForHoudini DESTINATION ${HOUDINI_INSTDIR})

if (MSVC)
    set_target_properties(ZibraVDBForHoudini PROPERTIES
        VS_DEBUGGER_COMMAND "${HOUDINI_PATH}/bin/houdini${CMAKE_EXECUTABLE_SUFFIX}"
        VS_DEBUGGER_COMMAND_ARGUMENTS "${CMAKE_CURRENT_SOURCE_DIR}/HIP/HDAs.hip"
        )
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        PROPERTY VS_STARTUP_PROJECT ZibraVDBForHoudini)
endif()
