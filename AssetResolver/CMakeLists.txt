cmake_minimum_required(VERSION 3.25)

project(ZibraVDBAssetResolver)

set(RESOLVER_HEADERS
    src/ZibraVDBAssetResolver.h
    src/DecompressionHelper.h
)

set(RESOLVER_SOURCES
    src/ZibraVDBAssetResolver.cpp
    src/DecompressionHelper.cpp
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

# Use parent project's output directory structure
if(DEFINED ZIBRAVDB_OUTPUT_PATH)
    set(ASSET_RESOLVER_OUTPUT_PATH "${ZIBRAVDB_OUTPUT_PATH}/dso/usd")
else()
    set(ASSET_RESOLVER_OUTPUT_PATH "${OUTPUT_PATH}/dso/usd")
endif()

# If RUNTIME_OUTPUT_DIRECTORY/LIBRARY_OUTPUT_DIRECTORY does not have generator expressions
# Configuration name will get automatically appended to it
# "$<0:>" at the end is empty generator expression
# Hence it will not append configuration name to the path
set_target_properties(ZibraVDBResolver PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${ASSET_RESOLVER_OUTPUT_PATH}$<0:>
    RUNTIME_OUTPUT_DIRECTORY ${ASSET_RESOLVER_OUTPUT_PATH}$<0:>
    PREFIX ""
)

add_custom_target(ZibraVDBResolverPluginsCopy
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/usd_plugins
    ${ASSET_RESOLVER_OUTPUT_PATH}/../usd_plugins
)
add_dependencies(ZibraVDBResolver ZibraVDBResolverPluginsCopy)
