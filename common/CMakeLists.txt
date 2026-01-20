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

target_include_directories(ZibraVDBCommon
    PUBLIC ${CMAKE_SOURCE_DIR}/external/Include
    PUBLIC ${CMAKE_SOURCE_DIR}/SDK/include)

target_compile_definitions(ZibraVDBCommon PUBLIC
    ZIB_NETWORK_REQUEST_BACKEND_${ZIB_NETWORK_REQUEST_BACKEND}=1
    ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1)

target_precompile_headers(ZibraVDBCommon PRIVATE src/PrecompiledHeader.h)
target_link_libraries(ZibraVDBCommon PUBLIC Houdini)

function(setup_common_houdini_dependencies target)
    message(STATUS "Setting up dependencies for ${target}")
    target_link_libraries(${target} PRIVATE Houdini ZibraVDBCommon)

    if("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
        if(${Houdini_VERSION_MAJOR} GREATER_EQUAL 21)
            set(HOUDINI_BOOST_PYTHON_TARGET "pxr_python")
        else()
            set(HOUDINI_BOOST_PYTHON_TARGET "hboost_python")
        endif()

        target_link_libraries(${target} PRIVATE
            Houdini::Dep::openvdb_sesi
            Houdini::Dep::${HOUDINI_BOOST_PYTHON_TARGET}
            Houdini::Dep::pxr_tf
            Houdini::Dep::pxr_sdf
            Houdini::Dep::pxr_usd
            Houdini::Dep::pxr_usdVol
        )
        if(NOT DEFINED _houdini_python_version)
            message(FATAL_ERROR "Houdini Python version not detected")
        endif()

        cmake_print_variables(_houdini_python_version)
        find_package(Python ${_houdini_python_version} EXACT
            COMPONENTS Interpreter Development
            REQUIRED
        )
        if(NOT Python_LIBRARY_DEBUG)
            message(FATAL_ERROR "Python ${python_version} was found, but no debug library is available.")
        endif()

        target_link_libraries(${target} PRIVATE Python::Python)
    elseif("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
        target_link_libraries(${target} PRIVATE "-framework Python")
    endif()
    # For Linux all the dependencies already included in Houdini target and no need to link anything explicitly
endfunction()
