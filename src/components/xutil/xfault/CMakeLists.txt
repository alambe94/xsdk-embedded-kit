set(XFAULT_SOURCES
    src/xfault.c
    src/xfault_cortex_r5.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    enable_language(ASM)
    list(APPEND XFAULT_SOURCES src/xfault_cortex_r5.S)
    set_source_files_properties(src/xfault_cortex_r5.S PROPERTIES LANGUAGE ASM)
endif()

add_library(xe_sdk_xfault STATIC
    ${XFAULT_SOURCES}
)

add_library(xsdk::xfault ALIAS xe_sdk_xfault)

target_include_directories(xe_sdk_xfault PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xe_sdk_xfault PUBLIC xsdk::xutil)

xsdk_apply_target_warnings(xe_sdk_xfault)

if(xSDK_BUILD_TESTS)
    add_subdirectory(tests)
endif()
