cmake_minimum_required(VERSION 3.25)

project( ZibraVDBAssetResolver )

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
if( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    # We need this to make sure that compile does not strip AR_DEFINE_RESOLVER(..) definitions.
    target_compile_options( ZibraVDBResolver PRIVATE "/Zc:inline-" )
endif()

target_link_libraries(ZibraVDBResolver PRIVATE Houdini ZibraVDBCommon)
if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    # Select boost python target based on Houdini version
    if(${Houdini_VERSION_MAJOR} GREATER_EQUAL 21)
        set(HOUDINI_BOOST_PYTHON_TARGET "pxr_python")
    else()
        set(HOUDINI_BOOST_PYTHON_TARGET "hboost_python")
    endif()
    
    target_link_libraries(ZibraVDBResolver PRIVATE
            Houdini::Dep::openvdb_sesi
            Houdini::Dep::${HOUDINI_BOOST_PYTHON_TARGET}

            Houdini::Dep::pxr_ar
            Houdini::Dep::pxr_arch
            Houdini::Dep::pxr_tf
    )
    if(DEFINED _houdini_python_version)
        set(PYTHON_TARGET_NAME "python${_houdini_python_version}")
        if(TARGET Houdini::Dep::${PYTHON_TARGET_NAME})
            target_link_libraries(ZibraVDBResolver PRIVATE Houdini::Dep::${PYTHON_TARGET_NAME})
            message(STATUS "ZibraVDBResolver linking with Houdini::Dep::${PYTHON_TARGET_NAME}")
        else()
            message(WARNING "ZibraVDBResolver: Target Houdini::Dep::${PYTHON_TARGET_NAME} not found")
        endif()

        # link with debug Python if available
        if(EXISTS ${PYTHON_DEBUG_LIB_PATH} AND EXISTS ${PYTHON_DEBUG_INCLUDE_PATH})
            target_link_libraries(ZibraVDBResolver PRIVATE
                    $<$<CONFIG:Debug>:${PYTHON_DEBUG_LIB_PATH}>
            )
            target_include_directories(ZibraVDBResolver SYSTEM PRIVATE
                    $<$<CONFIG:Debug>:${PYTHON_DEBUG_INCLUDE_PATH}>
            )
            message(STATUS "ZibraVDBForHoudini will use debug Python library for Debug builds: ${PYTHON_DEBUG_LIB_PATH}")
        else()
            message(STATUS "Debug Python not found, using Houdini's Python for all configurations")
        endif()

        #TODO remove hardcoded path
        set(TBB_DEBUG_LIB_PATH "C:/Users/sskap/Downloads/oneapi-tbb-2022.2.0-win/oneapi-tbb-2022.2.0/lib/intel64/vc14/tbb_debug.lib")
        set(TBB_DEBUG_MALLOC_PATH "C:/Users/sskap/Downloads/oneapi-tbb-2022.2.0-win/oneapi-tbb-2022.2.0/lib/intel64/vc14/tbbmalloc_debug.lib")
        if(EXISTS ${TBB_DEBUG_LIB_PATH})
            target_link_libraries(ZibraVDBResolver PRIVATE
                $<$<CONFIG:Debug>:${TBB_DEBUG_LIB_PATH}>
                $<$<CONFIG:Debug>:${TBB_DEBUG_MALLOC_PATH}>
            )
            message(STATUS "ZibraVDBResolver will use debug TBB for Debug builds: ${TBB_DEBUG_LIB_PATH}")
        endif()
    else()
        message(FATAL_ERROR "ZibraVDBResolver: Houdini Python version not detected")
    endif()
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    target_link_libraries(ZibraVDBResolver PRIVATE "-framework Python")
endif ()

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