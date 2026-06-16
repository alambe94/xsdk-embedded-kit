add_library(xboot STATIC
    src/xboot_core.c
    src/xboot_storage.c
    src/xboot_partition.c
    src/xboot_image.c
    src/xboot_verify.c
    src/xboot_handoff.c
)

add_library(xsdk::xboot ALIAS xboot)

target_include_directories(xboot PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xboot PUBLIC xsdk::xutil)

xsdk_apply_target_warnings(xboot)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

if(xSDK_SOC_AM243X)
    add_subdirectory(port/hil/am243x)
endif()
