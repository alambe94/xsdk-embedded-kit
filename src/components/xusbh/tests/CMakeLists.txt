add_subdirectory(fake)

xsdk_add_qemu_test(TARGET xusbh_qemu_realview
    SOURCES     qemu/main.c
    LIBS        xrtos xsdk::xusbh
    BOARD       REALVIEW_PB_A8
    OUTPUT      xusbh_qemu_realview
    PASS_STRING "ALL XUSBH TESTS PASSED SUCCESSFULLY"
)
