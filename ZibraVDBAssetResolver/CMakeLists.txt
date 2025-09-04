cmake_minimum_required(VERSION 3.25)

project( ZibraVDBAssetResolver )

set(RESOLVER_HEADERS
    src/ZibraVDBAssetResolver.h

    ../src/utils/Helpers.h
    ../src/licensing/LicenseManager.h
    ../src/bridge/LibraryUtils.h
    ../src/SOP/DecompressorManager/DecompressorManager.h
)

set(RESOLVER_SOURCES
    src/ZibraVDBAssetResolver.cpp

    ../src/utils/Helpers.cpp
    ../src/licensing/LicenseManager.cpp
    ../src/bridge/LibraryUtils.cpp
    ../src/SOP/DecompressorManager/DecompressorManager.cpp
)

add_library(zibraVDBResolver SHARED ${RESOLVER_HEADERS} ${RESOLVER_SOURCES})
target_precompile_headers(zibraVDBResolver PRIVATE src/PrecompiledHeader.h)
target_include_directories(zibraVDBResolver PRIVATE ../external/Include ../SDK/include src ../src)
target_compile_definitions(zibraVDBResolver PRIVATE
        ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1
        ZIBRAVDBRESOLVER_EXPORTS
        #TODO double-check if we actually need this?
        ZIB_NETWORK_REQUEST_BACKEND_CURL=1
        ZIB_NO_UI_ERROR=1
)

if( CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" )
    target_compile_options( zibraVDBResolver PRIVATE "/Zc:inline-" )
elseif( CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang" )
    # TODO this is temporary hack. Linux wants resolver lib name to match exactly the one specified in plugInfo (no extension)
    set_target_properties(zibraVDBResolver PROPERTIES SUFFIX "")
endif()

target_link_libraries(zibraVDBResolver PRIVATE Houdini HoudiniThirdParty)
if("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    #TODO find the correct way to link python
#    target_link_libraries(zibraVDBResolver PRIVATE /Applications/Houdini/Houdini21.0.440/Frameworks/Python.framework/Versions/3.11/lib/libpython3.11.dylib)
#    target_link_libraries(zibraVDBResolver PRIVATE /Applications/Houdini/Houdini21.0.440/Frameworks/Python.framework/Versions/Current/lib/libpython3.11.dylib)
#    target_link_libraries(zibraVDBResolver PRIVATE /Applications/Houdini/Houdini21.0.440/Frameworks/Houdini.framework/Resources/Frameworks/Python.framework/Versions/Current/lib/libpython3.11.dylib)
    target_link_libraries(zibraVDBResolver PRIVATE ${HOUDINI_PATH}/Frameworks/Python.framework/Versions/Current/lib/libpython3.11.dylib)
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

# Copy plugInfo files to the output directory structure
add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd_plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/usd_plugins/plugInfo.json" "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd_plugins/plugInfo.json"
)

add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd_plugins/zibraVDBResolver/resources"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/usd_plugins/zibraVDBResolver/resources/plugInfo.json" "${ASSET_RESOLVER_OUTPUT_PATH}/dso/usd_plugins/zibraVDBResolver/resources/plugInfo.json"
)