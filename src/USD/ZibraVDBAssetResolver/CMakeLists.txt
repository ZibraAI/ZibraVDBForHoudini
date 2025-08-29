# USD Asset Resolver - included as subdirectory

if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    add_compile_options(/Zc:inline-)
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "LINUX")
    # Linux/GCC equivalent: prevent stripping of static initialization symbols
    add_compile_options(-fno-inline-functions-called-once)
    add_link_options(-Wl,--no-as-needed)
endif()

set(RESOLVER_HEADERS
    src/resolver.h
    src/debugCodes.h

    ../../utils/Helpers.h
    ../../licensing/LicenseManager.h
    ../../bridge/LibraryUtils.h
    ../../SOP/DecompressorManager/DecompressorManager.h
)

set(RESOLVER_SOURCES
    src/resolver.cpp
    src/debugCodes.cpp

    ../../utils/Helpers.cpp
    ../../licensing/LicenseManager.cpp
    ../../bridge/LibraryUtils.cpp
    ../../SOP/DecompressorManager/DecompressorManager.cpp
)

add_library(zibraVDBResolver SHARED ${RESOLVER_HEADERS} ${RESOLVER_SOURCES})
target_precompile_headers(zibraVDBResolver PRIVATE src/PrecompiledHeader.h)
target_compile_definitions(zibraVDBResolver PRIVATE
        ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1
        ZIBRAVDBRESOLVER_EXPORTS
        ZIB_NETWORK_REQUEST_BACKEND_CURL=1
        ZIB_NO_UI_ERROR=1
)

add_compile_options(-O0 -g -fno-inline -fno-inline-functions -fno-inline-functions-called-once)
add_link_options(-Wl,-all_load -Wl,-no_dead_strip_inits_and_terms)
set_target_properties(zibraVDBResolver
        PROPERTIES
        XCODE_ATTRIBUTE_DEAD_CODE_STRIPPING NO
        XCODE_ATTRIBUTE_PRESERVE_DEAD_CODE_INITS_AND_TERMS YES
        XCODE_ATTRIBUTE_COPYING_PRESERVES_HFS_DATA YES
        XCODE_ATTRIBUTE_KEEP_PRIVATE_EXTERNS NO)


target_include_directories(zibraVDBResolver
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../external/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../SDK/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(zibraVDBResolver PRIVATE Houdini)

if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    target_link_libraries(zibraVDBResolver PRIVATE
        Houdini::Dep::openvdb_sesi

        Houdini::Dep::pxr_arch
        Houdini::Dep::pxr_tf
        Houdini::Dep::pxr_ar
    )
    if(DEFINED _houdini_python_version)
        set(PYTHON_TARGET_NAME "python${_houdini_python_version}")
        if(TARGET Houdini::Dep::${PYTHON_TARGET_NAME})
            target_link_libraries(zibraVDBResolver PRIVATE Houdini::Dep::${PYTHON_TARGET_NAME})
            message(STATUS "ZibraVDBResolver linking with Houdini::Dep::${PYTHON_TARGET_NAME}")
        else()
            message(WARNING "ZibraVDBResolver: Target Houdini::Dep::${PYTHON_TARGET_NAME} not found")
        endif()
    else()
        message(WARNING "ZibraVDBResolver: Houdini Python version not detected")
    endif()
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    set(PYTHON_TARGET_NAME "python${_houdini_python_version}")
    # target_link_libraries(zibraVDBResolver PRIVATE Houdini::Dep::pxr_python)
    # target_link_libraries(ZibraVDBForHoudini PRIVATE ${HOUDINI_PATH}/Frameworks/Python.framework/Versions/3.11/lib/libpython3.11.dylib)
    target_link_libraries(zibraVDBResolver PRIVATE /Applications/Houdini/Current/Frameworks/Python.framework/Versions/3.11/lib/libpython3.11.dylib)
endif()

if(DEFINED ZIBRAVDB_OUTPUT_PATH)
    set(ASSET_RESOLVER_OUTPUT_PATH ${ZIBRAVDB_OUTPUT_PATH})
else()
    set(ASSET_RESOLVER_OUTPUT_PATH ${OUTPUT_PATH})
endif()

cmake_print_variables(ASSET_RESOLVER_OUTPUT_PATH)

add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${ASSET_RESOLVER_OUTPUT_PATH}/zibra/0/lib
)

add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:zibraVDBResolver>
    ${ASSET_RESOLVER_OUTPUT_PATH}/zibra/0/lib
)

add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${ASSET_RESOLVER_OUTPUT_PATH}/zibra/0/resources
)

if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    add_custom_command(TARGET zibraVDBResolver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.win.json
        ${ASSET_RESOLVER_OUTPUT_PATH}/zibra/0/resources/plugInfo.json
    )
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "LINUX")
    add_custom_command(TARGET zibraVDBResolver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.linux.json
        ${ASSET_RESOLVER_OUTPUT_PATH}/zibra/0/resources/plugInfo.json
    )
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    add_custom_command(TARGET zibraVDBResolver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.mac.json
        ${ASSET_RESOLVER_OUTPUT_PATH}/zibra/0/resources/plugInfo.json
    )
endif()