add_subdirectory(fake)

xsdk_add_qemu_test(TARGET xusbd_qemu_realview
    SOURCES     qemu/main.c
    LIBS        xrtos xsdk::xusbd
    BOARD       REALVIEW_PB_A8
    OUTPUT      xusbd_qemu_realview
    PASS_STRING "ALL XUSBD TESTS PASSED SUCCESSFULLY"
)
