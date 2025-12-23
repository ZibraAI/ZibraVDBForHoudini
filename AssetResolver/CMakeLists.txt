cmake_minimum_required(VERSION 3.25)

project(ZibraVDBAssetResolver)

set(RESOLVER_HEADERS
    src/PrecompiledHeader.h
    src/ZibraVDBAssetResolver.h
    src/decompression/DecompressionHelper.h
    src/decompression/DecompressionSequenceItem.h
)

set(RESOLVER_SOURCES
    src/ZibraVDBAssetResolver.cpp
    src/decompression/DecompressionHelper.cpp
    src/decompression/DecompressionSequenceItem.cpp
)

add_library(ZibraVDBResolver SHARED ${RESOLVER_HEADERS} ${RESOLVER_SOURCES})
target_precompile_headers(ZibraVDBResolver PRIVATE src/PrecompiledHeader.h)
target_include_directories(ZibraVDBResolver PRIVATE src)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # We need this to make sure that compile does not strip AR_DEFINE_RESOLVER(..) definitions.
    target_compile_options(ZibraVDBResolver PRIVATE "/Zc:inline-")
endif()

setup_common_houdini_dependencies(ZibraVDBResolver)

if("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    target_link_libraries(ZibraVDBResolver PRIVATE
        Houdini::Dep::pxr_ar
        Houdini::Dep::pxr_arch
        Houdini::Dep::pxr_tf
    )
endif()

if (${LABS_BUILD})
    # If RUNTIME_OUTPUT_DIRECTORY/LIBRARY_OUTPUT_DIRECTORY does not have generator expressions
    # Configuration name will get automatically appended to it
    # "$<0:>" at the end is empty generator expression
    # Hence it will not append configuration name to the path
    set_target_properties(ZibraVDBResolver PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${OUTPUT_PATH}/dso/usd$<0:>
        RUNTIME_OUTPUT_DIRECTORY ${OUTPUT_PATH}/dso/usd$<0:>
        PREFIX ""
    )
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.json.in
    ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json
    @ONLY
)

set(ZIBRAVDB_ASSET_RESOLVER_PLUGIN_CONFIG ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json PARENT_SCOPE)

if (${LABS_BUILD})
    add_custom_target(ZibraVDBCopyAssetResolverPluginConfig
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/usd_plugins
        ${OUTPUT_PATH}/dso/usd_plugins
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json
        ${OUTPUT_PATH}/dso/usd_plugins/ZibraVDBResolver/resources/plugInfo.json
    )
    add_dependencies(ZibraVDBResolver ZibraVDBCopyAssetResolverPluginConfig)
endif ()
