cmake_minimum_required(VERSION 3.25)

project(ZibraVDBCommon)

set(COMMON_PUBLIC_HEADERS
    Globals.h
    utils/Helpers.h
    utils/MetadataHelper.h
    utils/DecompressorManager.h
    licensing/LicenseManager.h
    bridge/LibraryUtils.h
    ui/PluginManagementWindow.h
)

set(COMMON_INTERNAL_HEADERS
    PrecompiledHeader.h
    utils/GAAttributesDump.h
    bridge/UpdateCheck.h
    bridge/networking/NetworkRequest.h
    ui/MessageBox.h
)

set(COMMON_SOURCES
    utils/Helpers.cpp
    utils/MetadataHelper.cpp
    utils/GAAttributesDump.cpp
    utils/DecompressorManager.cpp
    licensing/LicenseManager.cpp
    bridge/LibraryUtils.cpp
    bridge/UpdateCheck.cpp
    bridge/networking/NetworkRequest.cpp
    ui/MessageBox.cpp
    ui/PluginManagementWindow.cpp
)

add_library(ZibraVDBCommon STATIC)
target_sources(ZibraVDBCommon
    PUBLIC FILE_SET HEADERS
        FILES ${COMMON_PUBLIC_HEADERS}
    PRIVATE
        ${COMMON_INTERNAL_HEADERS}
        ${COMMON_SOURCES}
)

target_include_directories(ZibraVDBCommon
    PUBLIC ${CMAKE_SOURCE_DIR}/external/Include
    PUBLIC ${CMAKE_SOURCE_DIR}/SDK/include)

target_compile_definitions(ZibraVDBCommon PUBLIC
    ZIB_NETWORK_REQUEST_BACKEND_${ZIB_NETWORK_REQUEST_BACKEND}=1
    ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1)

target_precompile_headers(ZibraVDBCommon PRIVATE PrecompiledHeader.h)
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
