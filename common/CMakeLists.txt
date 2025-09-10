cmake_minimum_required(VERSION 3.25)

project(ZibraVDBCommon)

# Common source files
set(COMMON_HEADERS
    utils/Helpers.h
    licensing/LicenseManager.h
    bridge/LibraryUtils.h
    bridge/UpdateCheck.h
    bridge/networking/NetworkRequest.h
    SOP/DecompressorManager/DecompressorManager.h
    ui/MessageBox.h
    ui/PluginManagementWindow.h
)

set(COMMON_SOURCES
    utils/Helpers.cpp
    licensing/LicenseManager.cpp
    bridge/LibraryUtils.cpp
    bridge/UpdateCheck.cpp
    bridge/networking/NetworkRequest.cpp
    SOP/DecompressorManager/DecompressorManager.cpp
    ui/MessageBox.cpp
    ui/PluginManagementWindow.cpp
)

# Create common static library
add_library(ZibraVDBCommon STATIC ${COMMON_HEADERS} ${COMMON_SOURCES})

# Public interface - these directories will be available to any target linking to ZibraVDBCommon
target_include_directories(ZibraVDBCommon 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../external/Include
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../SDK/include
    PRIVATE "${HOUDINI_INCLUDE_DIR}")

# Public compile definitions
target_compile_definitions(ZibraVDBCommon PUBLIC 
    ZIB_NETWORK_REQUEST_BACKEND_${ZIB_NETWORK_REQUEST_BACKEND}=1
    ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1)

# Use common precompiled header
target_precompile_headers(ZibraVDBCommon PRIVATE PrecompiledHeader.h)

# Link to Houdini (PUBLIC so consumers get it too)
target_link_libraries(ZibraVDBCommon PUBLIC Houdini)