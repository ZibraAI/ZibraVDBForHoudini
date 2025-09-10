cmake_minimum_required(VERSION 3.25)

project( ZibraVDBAssetResolver )

set(RESOLVER_HEADERS
    src/ZibraVDBAssetResolver.h
)

set(RESOLVER_SOURCES
    src/ZibraVDBAssetResolver.cpp
)

add_library(zibraVDBResolver SHARED ${RESOLVER_HEADERS} ${RESOLVER_SOURCES})
target_precompile_headers(zibraVDBResolver PRIVATE ../common/PrecompiledHeader.h)
target_include_directories(zibraVDBResolver PRIVATE src)
target_compile_definitions(zibraVDBResolver PRIVATE
        ZIBRAVDBRESOLVER_EXPORTS
        ZIB_NO_UI_ERROR=1
)
if( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    target_compile_options( zibraVDBResolver PRIVATE "/Zc:inline-" )
endif()

target_link_libraries(zibraVDBResolver PRIVATE Houdini ZibraVDBCommon)
if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    # Select boost python target based on Houdini version
    if(${Houdini_VERSION_MAJOR} GREATER_EQUAL 21)
        set(HOUDINI_BOOST_PYTHON_TARGET "pxr_python")
    else()
        set(HOUDINI_BOOST_PYTHON_TARGET "hboost_python")
    endif()
    
    target_link_libraries(zibraVDBResolver PRIVATE
            Houdini::Dep::openvdb_sesi
            Houdini::Dep::${HOUDINI_BOOST_PYTHON_TARGET}

            Houdini::Dep::pxr_ar
            Houdini::Dep::pxr_arch
            Houdini::Dep::pxr_tf
    )
    if(DEFINED _houdini_python_version)
        set(PYTHON_TARGET_NAME "python${_houdini_python_version}")
        if(TARGET Houdini::Dep::${PYTHON_TARGET_NAME})
            target_link_libraries(zibraVDBResolver PRIVATE Houdini::Dep::${PYTHON_TARGET_NAME})
            message(STATUS "zibraVDBResolver linking with Houdini::Dep::${PYTHON_TARGET_NAME}")
        else()
            message(WARNING "zibraVDBResolver: Target Houdini::Dep::${PYTHON_TARGET_NAME} not found")
        endif()
    else()
        message(WARNING "zibraVDBResolver: Houdini Python version not detected")
    endif()
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    # TODO check if we can just use Houdini::Dep::python
    target_link_libraries(zibraVDBResolver PRIVATE ${HOUDINI_PATH}/Frameworks/Python.framework/Versions/Current/lib/libpython${_houdini_python_version}.dylib)
endif ()

# Use parent project's output directory structure
if(DEFINED ZIBRAVDB_OUTPUT_PATH)
    set(ASSET_RESOLVER_OUTPUT_PATH ${ZIBRAVDB_OUTPUT_PATH})
else()
    set(ASSET_RESOLVER_OUTPUT_PATH ${OUTPUT_PATH})
endif()

set_target_properties(zibraVDBResolver PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd"
    RUNTIME_OUTPUT_DIRECTORY "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd"
    LIBRARY_OUTPUT_DIRECTORY_DEBUG "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd"
    PREFIX ""
)

add_custom_target(ZibraVDBResolverPluginsCopy
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/usd_plugins
    ${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd_plugins
)
add_dependencies(zibraVDBResolver ZibraVDBResolverPluginsCopy)