# Tests that use test_xusbh_helpers.h include the local test directory.
function(xsdk_add_xusbh_test stem)
    xsdk_add_test(xusbh_${stem}_test
        SOURCES test_xusbh_${stem}.c
        LIBS    xsdk::xusbh
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARGN}
    )
endfunction()

xsdk_add_xusbh_test(core)
xsdk_add_xusbh_test(class)
xsdk_add_xusbh_test(hcd)
xsdk_add_xusbh_test(hid)
xsdk_add_xusbh_test(msc)
xsdk_add_xusbh_test(transfer)
xsdk_add_xusbh_test(descriptor)
xsdk_add_xusbh_test(port)
xsdk_add_xusbh_test(enum)

xsdk_add_test(xusbh_fake_hcd_port_test
    SOURCES
        test_xusbh_fake_hcd_port.c
        ../../port/fake/xusbh_drv.c
    LIBS xsdk::xutil xsdk::xusb_common
    INCLUDES
        ../../port/fake
        ../../include
)
