xsdk_add_test(xusb_shared_helpers_test
    SOURCES fake/test_shared_helpers.c
    LIBS    xsdk::xusb_common
)
