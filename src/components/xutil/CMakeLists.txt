add_library(xe_sdk_utils INTERFACE)

add_library(xsdk::xutil ALIAS xe_sdk_utils)

# Shared utilities (xreturn.h, xassert.h, xlog.h, xbytes.h) are header-only
# from the consumer's perspective. Expose the xutil root as a public include.
target_include_directories(xe_sdk_utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Pass xSDK_ENABLE_ASSERT=1 to all modules in debug / host-test builds.
# Cross-compile builds leave it unset (0) for production firmware.
if(xSDK_ENABLE_ASSERT)
    target_compile_definitions(xe_sdk_utils INTERFACE xSDK_ENABLE_ASSERT=1)
endif()

add_subdirectory(xtrace)
add_subdirectory(xfault)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    xsdk_add_qemu_test(TARGET xutil_qemu_realview
        SOURCES     tests/qemu/main.c
        LIBS        xrtos xsdk::xtrace xsdk::xfault
        BOARD       REALVIEW_PB_A8
        OUTPUT      xutil_qemu_realview
        PASS_STRING "ALL XUTIL TESTS PASSED SUCCESSFULLY")

    xsdk_add_qemu_test(TARGET xtrace_qemu_realview
        SOURCES     xtrace/tests/qemu/main.c
        LIBS        xrtos xsdk::xtrace
        BOARD       REALVIEW_PB_A8
        OUTPUT      xtrace_qemu_realview
        PASS_STRING "ALL XTRACE TESTS PASSED SUCCESSFULLY")

    xsdk_add_qemu_test(TARGET xfault_qemu_realview
        SOURCES     xfault/tests/qemu/main.c
        LIBS        xrtos xsdk::xfault
        BOARD       REALVIEW_PB_A8
        OUTPUT      xfault_qemu_realview
        PASS_STRING "ALL XFAULT TESTS PASSED SUCCESSFULLY")
endif()

if(xSDK_BUILD_TESTS)
    add_subdirectory(tests)
endif()
