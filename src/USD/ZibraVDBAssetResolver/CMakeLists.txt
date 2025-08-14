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
        Houdini::Dep::python3.11
        Houdini::Dep::openvdb_sesi

        Houdini::Dep::pxr_arch
        Houdini::Dep::pxr_tf
        Houdini::Dep::pxr_ar
    )
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    # macOS requires explicit Python library linking for undefined symbol resolution
    if(TARGET Houdini::Dep::python3.11)
        target_link_libraries(zibraVDBResolver PRIVATE Houdini::Dep::python3.11)
    elseif(TARGET Houdini::Dep::python)
        target_link_libraries(zibraVDBResolver PRIVATE Houdini::Dep::python)
    else()
        find_library(PYTHON_LIB NAMES
                python3.11 libpython3.11
                python3.10 libpython3.10
                python3.9 libpython3.9
                python3 libpython3
                python Python
        )
        if(PYTHON_LIB)
            target_link_libraries(zibraVDBResolver PRIVATE ${PYTHON_LIB})
        endif()
    endif()

    if(TARGET Houdini::Dep::hboost_python)
        target_link_libraries(zibraVDBResolver PRIVATE Houdini::Dep::hboost_python)
    elseif(TARGET Houdini::Dep::hboost_python311)
        target_link_libraries(zibraVDBResolver PRIVATE Houdini::Dep::hboost_python311)
    endif()
endif()

# Copy zibraVDBResolver library to $OUTPUT_PATH/zibra/0/lib
add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${OUTPUT_PATH}/zibra/0/lib
)

add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:zibraVDBResolver>
    ${OUTPUT_PATH}/zibra/0/lib/
)

# Copy plugInfo.json files to $OUTPUT_PATH/zibra/0/resources
add_custom_command(TARGET zibraVDBResolver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    ${OUTPUT_PATH}/zibra/0/resources
)

if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    add_custom_command(TARGET zibraVDBResolver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.win.json
        ${OUTPUT_PATH}/zibra/0/resources/plugInfo.json
    )
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "LINUX")
    add_custom_command(TARGET zibraVDBResolver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.linux.json
        ${OUTPUT_PATH}/zibra/0/resources/plugInfo.json
    )
endif()