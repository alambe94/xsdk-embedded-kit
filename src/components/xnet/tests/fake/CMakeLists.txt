xsdk_add_test(xnet_foundation_test
    SOURCES  test_xnet_foundation.c
    LIBS     xsdk::xnet
)

xsdk_add_test(xnet_dns_test
    SOURCES  test_xnet_dns.c
    LIBS     xsdk::xnet
)
