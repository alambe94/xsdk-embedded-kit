xsdk_add_test(xboot_policy_test SOURCES test_xboot_policy.c LIBS xsdk::xboot)

xsdk_add_test(xboot_storage_fake_test
    SOURCES
        test_xboot_storage_fake.c
        ../../port/fake/xboot_storage_fake.c
    LIBS
        xsdk::xboot
)
target_include_directories(xboot_storage_fake_test PRIVATE ../../port/fake)

xsdk_add_test(xboot_verify_test SOURCES test_xboot_verify.c LIBS xsdk::xboot)
xsdk_add_test(xboot_partition_test SOURCES test_xboot_partition.c LIBS xsdk::xboot)
xsdk_add_test(xboot_image_test SOURCES test_xboot_image.c LIBS xsdk::xboot)

xsdk_add_test(xboot_handoff_test
    SOURCES
        test_xboot_handoff.c
        ../../port/fake/xboot_port_fake.c
    LIBS
        xsdk::xboot
)
target_include_directories(xboot_handoff_test PRIVATE ../../port/fake)

