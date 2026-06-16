add_library(xfs STATIC
    src/xfs_core.c
    src/xfs_fat32.c
    src/xfs_fat32_bpb.c
    src/xfs_fat32_cluster.c
    src/xfs_fat32_directory.c
    src/xfs_format.c
    src/xfs_cache.c
    src/xfs_path.c
    src/xfs_file.c
    src/xfs_directory.c
)

add_library(xsdk::xfs ALIAS xfs)

target_include_directories(xfs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xfs PUBLIC xsdk::xutil xsdk::xtrace)

xsdk_apply_target_warnings(xfs)

# Port selection - set XFS_PORT to include a block device backend.
# Example: cmake -DXFS_PORT=ramdisk
if(DEFINED XFS_PORT)
    if(XFS_PORT STREQUAL "ramdisk")
        set(XFS_PORT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/port/fake/xfs_block_ramdisk.c")
        set(XFS_PORT_INC "${CMAKE_CURRENT_SOURCE_DIR}/port/fake")
    elseif(XFS_PORT STREQUAL "usbmsc")
        set(XFS_PORT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/port/hil/xfs_block_usbmsc.c")
        set(XFS_PORT_INC "${CMAKE_CURRENT_SOURCE_DIR}/port/hil")
    else()
        message(FATAL_ERROR "xfs: unknown XFS_PORT=${XFS_PORT}")
    endif()

    target_sources(xfs PRIVATE ${XFS_PORT_SRC})
    target_include_directories(xfs PUBLIC ${XFS_PORT_INC})
    if(XFS_PORT STREQUAL "usbmsc")
        if(TARGET xsdk::xusbh)
            target_link_libraries(xfs PUBLIC xsdk::xusbh)
        else()
            message(FATAL_ERROR "xfs: XFS_PORT=usbmsc requires xSDK_BUILD_XUSB_HOST=ON")
        endif()
    endif()
else()
    message(STATUS "xfs: no port selected - set XFS_PORT (e.g. ramdisk) to include a block device backend")
endif()

xsdk_add_qemu_test(TARGET xfs_qemu_realview
    SOURCES
        tests/qemu/main.c
        port/fake/xfs_block_ramdisk.c
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/port/fake
    LIBS        xrtos xsdk::xfs
    BOARD       REALVIEW_PB_A8
    OUTPUT      xfs_qemu_realview
    PASS_STRING "ALL XFS TESTS PASSED SUCCESSFULLY")

xsdk_add_qemu_test(TARGET xfs_qemu_realview_trace
    SOURCES
        tests/qemu/main.c
        port/fake/xfs_block_ramdisk.c
    INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/port/fake
    LIBS        xrtos xsdk::xfs xsdk::xtrace
    BOARD       REALVIEW_PB_A8
    DEFS        xTRACE_ENABLE=1 QEMU_TRACE_ENABLED
    OUTPUT      xfs_qemu_realview_trace
    PASS_STRING "ALL XFS TESTS PASSED SUCCESSFULLY"
    IS_TRACE)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
