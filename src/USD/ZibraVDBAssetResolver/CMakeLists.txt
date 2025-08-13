# USD Asset Resolver - included as subdirectory
set(RESOLVER_NAME zibraVDBResolver)
set(RESOLVER_CLASS_NAME ZibraVDBResolver)
set(RESOLVER_TARGET_LIB zibraVDBResolver)

if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    add_compile_options(/Zc:inline-)
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "LINUX")
    # Linux/GCC equivalent: prevent stripping of static initialization symbols
    add_compile_options(-fno-inline-functions-called-once)
    add_link_options(-Wl,--no-as-needed)
endif()

add_library(${RESOLVER_TARGET_LIB} SHARED)
set(RESOLVER_HEADERS
    src/resolver.h
    src/debugCodes.h
    src/DecompressorManager.h
)

set(RESOLVER_SOURCES
    src/resolver.cpp
    src/debugCodes.cpp
    src/DecompressorManager.cpp
)

list(APPEND RESOLVER_HEADERS ../../utils/Helpers.h)
list(APPEND RESOLVER_HEADERS ../../ui/MessageBox.h)
list(APPEND RESOLVER_HEADERS ../../licensing/LicenseManager.h)
list(APPEND RESOLVER_HEADERS ../../bridge/LibraryUtils.h)

list(APPEND RESOLVER_SOURCES ../../utils/Helpers.cpp)
list(APPEND RESOLVER_SOURCES ../../ui/MessageBox.cpp)
list(APPEND RESOLVER_SOURCES ../../licensing/LicenseManager.cpp)
list(APPEND RESOLVER_SOURCES ../../bridge/LibraryUtils.cpp)

target_sources(${RESOLVER_TARGET_LIB}
    PRIVATE ${RESOLVER_HEADERS}
    PRIVATE ${RESOLVER_SOURCES}
)

target_compile_definitions(${RESOLVER_TARGET_LIB} PRIVATE ZIB_TARGET_OS_${ZIB_TARGET_OS_NAME}=1)
target_compile_definitions(${RESOLVER_TARGET_LIB} PRIVATE ZIBRAVDBRESOLVER_EXPORTS)
target_compile_definitions(${RESOLVER_TARGET_LIB} PRIVATE ZIB_NETWORK_REQUEST_BACKEND_CURL=1)
target_precompile_headers(${RESOLVER_TARGET_LIB} PRIVATE src/PrecompiledHeader.h)

target_include_directories(${RESOLVER_TARGET_LIB}
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../external/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../SDK/include

    ${CMAKE_CURRENT_SOURCE_DIR}/src

    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(${RESOLVER_TARGET_LIB} PRIVATE Houdini)

if ("${ZIB_TARGET_OS_NAME}" STREQUAL "WIN")
    target_link_libraries(${RESOLVER_TARGET_LIB} PRIVATE
        Houdini::Dep::python3.11
        Houdini::Dep::openvdb_sesi

        Houdini::Dep::pxr_arch
        Houdini::Dep::pxr_tf
        Houdini::Dep::pxr_ar
    )
elseif ("${ZIB_TARGET_OS_NAME}" STREQUAL "MAC")
    # macOS requires explicit Python library linking for undefined symbol resolution
    if(TARGET Houdini::Dep::python3.11)
        target_link_libraries(${RESOLVER_TARGET_LIB} PRIVATE Houdini::Dep::python3.11)
    elseif(TARGET Houdini::Dep::python)
        target_link_libraries(${RESOLVER_TARGET_LIB} PRIVATE Houdini::Dep::python)
    else()
        find_library(PYTHON_LIB NAMES
                python3.11 libpython3.11
                python3.10 libpython3.10
                python3.9 libpython3.9
                python3 libpython3
                python Python
        )
        if(PYTHON_LIB)
            target_link_libraries(${RESOLVER_TARGET_LIB} PRIVATE ${PYTHON_LIB})
        endif()
    endif()

    if(TARGET Houdini::Dep::hboost_python)
        target_link_libraries(${RESOLVER_TARGET_LIB} PRIVATE Houdini::Dep::hboost_python)
    elseif(TARGET Houdini::Dep::hboost_python311)
        target_link_libraries(${RESOLVER_TARGET_LIB} PRIVATE Houdini::Dep::hboost_python311)
    endif()
endif()