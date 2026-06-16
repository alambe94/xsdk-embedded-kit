# Per-module wrapper: adds the ramdisk port source and include path.
function(xsdk_add_xfs_test stem)
    xsdk_add_test(xfs_${stem}_test
        SOURCES  test_xfs_${stem}.c ../../port/fake/xfs_block_ramdisk.c
        LIBS     xsdk::xfs
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../port/fake
        ${ARGN}
    )
endfunction()

xsdk_add_xfs_test(fat)
xsdk_add_xfs_test(path)
xsdk_add_xfs_test(file)

if(TARGET xsdk::xusbh)
    xsdk_add_test(xfs_usbmsc_test
        SOURCES
            test_xfs_usbmsc.c
            ../../port/hil/xfs_block_usbmsc.c
        LIBS
            xsdk::xfs
            xsdk::xusbh
        INCLUDES
            ${CMAKE_CURRENT_SOURCE_DIR}/../../port/hil
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../xusbh/tests/fake
    )
endif()
