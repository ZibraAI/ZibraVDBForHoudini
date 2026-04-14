cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ZIBRAVDB_PLUGIN_VERSION_MAJOR)
    message(FATAL_ERROR "ZIBRAVDB_VERSION_MAJOR must be specified")
endif()
if(NOT DEFINED ZIBRAVDB_PLUGIN_VERSION_MINOR)
    message(FATAL_ERROR "ZIBRAVDB_VERSION_MINOR must be specified")
endif()
if(NOT DEFINED ZIBRAVDB_PLUGIN_VERSION_PATCH)
    message(FATAL_ERROR "ZIBRAVDB_VERSION_PATCH must be specified")
endif()
if(NOT DEFINED ZIBRAVDB_PLUGIN_VERSION_TWEAK)
    message(FATAL_ERROR "ZIBRAVDB_VERSION_TWEAK must be specified")
endif()

if("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    set(ZIB_NETWORK_REQUEST_BACKEND "WINHTTP")
elseif("${ZIB_TARGET_OS_NAME}" STREQUAL "LINUX")
    set(ZIB_NETWORK_REQUEST_BACKEND "CURL")
elseif("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    set(ZIB_NETWORK_REQUEST_BACKEND "CURL")
else()
    message(FATAL_ERROR "Unsupported target OS: ${ZIB_TARGET_OS_NAME}")
endif()
cmake_print_variables(ZIB_NETWORK_REQUEST_BACKEND)

project(ZibraVDBCommon VERSION ${ZIBRAVDB_PLUGIN_VERSION_MAJOR}.${ZIBRAVDB_PLUGIN_VERSION_MINOR}.${ZIBRAVDB_PLUGIN_VERSION_PATCH}.${ZIBRAVDB_PLUGIN_VERSION_TWEAK})

configure_file(include/Globals.h.in include/Globals.h @ONLY)

add_library(ZibraVDBCommon STATIC)
target_sources(ZibraVDBCommon
    PUBLIC
    FILE_SET public_interface TYPE HEADERS
        BASE_DIRS include
        FILES
            include/Globals.h.in
            include/Types.h
            include/utils/Helpers.h
            include/utils/MetadataHelper.h
            include/utils/DecompressorManager.h
            include/licensing/LicenseManager.h
            include/bridge/LibraryUtils.h
            include/ui/PluginManagementWindow.h

    PUBLIC
    FILE_SET generated_interface TYPE HEADERS
        BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}/include
        FILES
            ${CMAKE_CURRENT_BINARY_DIR}/include/Globals.h

    PRIVATE
    FILE_SET private_headers TYPE HEADERS
        BASE_DIRS src
        FILES
            src/PrecompiledHeader.h
            src/utils/GAAttributesDump.h
            src/bridge/UpdateCheck.h
            src/bridge/networking/NetworkRequest.h
            src/ui/MessageBox.h
        
    PRIVATE
        src/main.cpp
        src/utils/Helpers.cpp
        src/utils/MetadataHelper.cpp
        src/utils/GAAttributesDump.cpp
        src/utils/DecompressorManager.cpp
        src/licensing/LicenseManager.cpp
        src/bridge/LibraryUtils.cpp
        src/bridge/UpdateCheck.cpp
        src/bridge/networking/NetworkRequest.cpp
        src/ui/MessageBox.cpp
        src/ui/PluginManagementWindow.cpp
)

target_include_directories(ZibraVDBCommon PUBLIC
    ${CMAKE_SOURCE_DIR}/external/Include
    ${CMAKE_SOURCE_DIR}/SDK/include
    "${HOUDINI_INCLUDE_DIR}")

target_compile_definitions(ZibraVDBCommon PUBLIC
    ZIB_NETWORK_REQUEST_BACKEND_${ZIB_NETWORK_REQUEST_BACKEND}=1
    ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1
    ZCE_NO_STATIC_API_DECL
    ZRHI_NO_STATIC_API_DECL)

target_precompile_headers(ZibraVDBCommon PRIVATE src/PrecompiledHeader.h)
target_link_libraries(ZibraVDBCommon PUBLIC Houdini HoudiniThirdParty)

# On Windows we also need debug Python library to build debug configuration
if("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    if(NOT DEFINED _houdini_python_version)
        message(FATAL_ERROR "Houdini Python version not detected")
    endif()

    cmake_print_variables(_houdini_python_version)
    if(${LABS_BUILD})
        cmake_print_variables(HOUDINI_PYTHON_BIN)
        set(Python_EXECUTABLE "${HOUDINI_PYTHON_BIN}")
    endif()

    find_package(Python ${_houdini_python_version} EXACT
        COMPONENTS Interpreter Development
    )

    if (NOT Python_FOUND)
        message(Warning "Python ${_houdini_python_version} was not found. You won't be able to build debug configuration")
        target_compile_definitions(ZibraVDBCommon PUBLIC ZIB_NO_DEBUG_PYTHON=1)
    elseif(NOT Python_LIBRARY_DEBUG)
        message(Warning "Python ${_houdini_python_version} was found, but no debug library is available. You won't be able to build debug configuration")
        target_compile_definitions(ZibraVDBCommon PUBLIC ZIB_NO_DEBUG_PYTHON=1)
    else()
        target_link_libraries(ZibraVDBCommon PUBLIC Python::Python)
        message(STATUS "Found Python ${_houdini_python_version} with debug library: ${Python_LIBRARY_DEBUG}" )
    endif()
endif()
