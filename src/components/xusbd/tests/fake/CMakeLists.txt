function(xsdk_add_xusbd_test stem)
    xsdk_add_test(xusbd_${stem}_test
        SOURCES  test_${stem}.c
        LIBS     xsdk::xusbd fff
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARGN}
    )
endfunction()

xsdk_add_xusbd_test(init)
xsdk_add_xusbd_test(class_registration)
xsdk_add_xusbd_test(descriptor)
xsdk_add_xusbd_test(routing)
xsdk_add_xusbd_test(bus_events)
xsdk_add_xusbd_test(standard_requests)
xsdk_add_xusbd_test(msc)
xsdk_add_xusbd_test(cdc)
xsdk_add_xusbd_test(hid)
xsdk_add_xusbd_test(dfu)
xsdk_add_xusbd_test(msc_bot)
xsdk_add_xusbd_test(win)
xsdk_add_xusbd_test(dfu_protocol)

xsdk_add_test(xusbd_fake_dcd_port_test
    SOURCES
        test_xusbd_fake_dcd_port.c
        ../../port/fake/xusbd_drv.c
    LIBS xsdk::xutil xsdk::xusb_common
    INCLUDES
        ../../port/fake
        ../../include
)
