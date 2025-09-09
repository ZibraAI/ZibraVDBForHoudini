cmake_minimum_required(VERSION 3.25)

project( ZibraVDBAssetResolver )

set(RESOLVER_HEADERS
    src/ZibraVDBAssetResolver.h
)

set(RESOLVER_SOURCES
    src/ZibraVDBAssetResolver.cpp
)

add_library(zibraVDBResolver SHARED ${RESOLVER_HEADERS} ${RESOLVER_SOURCES})
target_precompile_headers(zibraVDBResolver PRIVATE src/PrecompiledHeader.h)
target_include_directories(zibraVDBResolver PRIVATE src)
target_compile_definitions(zibraVDBResolver PRIVATE
        ZIBRAVDBRESOLVER_EXPORTS
        ZIB_NO_UI_ERROR=1
)
if( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    target_compile_options( zibraVDBResolver PRIVATE "/Zc:inline-" )
endif()

target_link_libraries(zibraVDBResolver PRIVATE Houdini HoudiniThirdParty ZibraVDBCommon)
if("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    # we still need to explicitly link the python lib for mac
    target_link_libraries(zibraVDBResolver PRIVATE ${HOUDINI_PATH}/Frameworks/Python.framework/Versions/Current/lib/libpython${_houdini_python_version}.dylib)
endif()

string(REPLACE "." "" _houdini_python_dotless_version ${_houdini_python_version})
if("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    target_link_options(zibraVDBResolver PRIVATE
            "$<$<CONFIG:Debug>:/NODEFAULTLIB:python${_houdini_python_dotless_version}_d.lib>"
    )
endif()

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

add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/usd_plugins" "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd_plugins"
)