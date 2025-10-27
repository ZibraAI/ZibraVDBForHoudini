cmake_minimum_required(VERSION 3.25)

project(ZibraVDBCommon)

add_library(ZibraVDBCommon STATIC)
target_sources(ZibraVDBCommon
    PUBLIC FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include
        FILES
            include/Globals.h
            include/Types.h
            include/utils/Helpers.h
            include/utils/MetadataHelper.h
            include/utils/DecompressorManager.h
            include/licensing/LicenseManager.h
            include/bridge/LibraryUtils.h
            include/ui/PluginManagementWindow.h
    PRIVATE
        # Private Headers
        PrecompiledHeader.h
        src/utils/GAAttributesDump.h
        src/bridge/UpdateCheck.h
        src/bridge/networking/NetworkRequest.h
        src/ui/MessageBox.h
        
        # Source Files
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
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PUBLIC ${CMAKE_SOURCE_DIR}/external/Include
    PUBLIC ${CMAKE_SOURCE_DIR}/SDK/include)

target_compile_definitions(ZibraVDBCommon PUBLIC
    ZIB_NETWORK_REQUEST_BACKEND_${ZIB_NETWORK_REQUEST_BACKEND}=1
    ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1
    LABS_BUILD=$<BOOL:${LABS_BUILD}>)

target_precompile_headers(ZibraVDBCommon PRIVATE PrecompiledHeader.h)
target_link_libraries(ZibraVDBCommon PUBLIC Houdini)

if(NOT LABS_BUILD AND "${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    target_link_libraries(ZibraVDBCommon PUBLIC
        Houdini::Dep::pxr_plug
        Houdini::Dep::pxr_tf
    )
endif()

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
