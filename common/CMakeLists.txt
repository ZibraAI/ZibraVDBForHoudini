cmake_minimum_required(VERSION 3.25)

project(ZibraVDBCommon)

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

add_library(ZibraVDBCommon STATIC ${COMMON_HEADERS} ${COMMON_SOURCES})
target_include_directories(ZibraVDBCommon
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../external/Include
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../SDK/include
    PRIVATE "${HOUDINI_INCLUDE_DIR}")

target_compile_definitions(ZibraVDBCommon PUBLIC
    ZIB_NETWORK_REQUEST_BACKEND_${ZIB_NETWORK_REQUEST_BACKEND}=1
    ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1)

target_precompile_headers(ZibraVDBCommon PRIVATE PrecompiledHeader.h)
target_link_libraries(ZibraVDBCommon PUBLIC Houdini)