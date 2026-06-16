# xNET CMake configuration
#

add_library(xnet STATIC
    src/xnet_core.c
    src/xnet_interface.c
    src/xnet_packet.c
    src/xnet_ethernet.c
    src/xnet_arp.c
    src/xnet_ipv4.c
    src/xnet_icmp.c
    src/xnet_udp.c
    src/xnet_timer.c
    src/xnet_dhcp.c
    src/xnet_dns.c
    port/fake/xnet_interface_fake.c
)

add_library(xsdk::xnet ALIAS xnet)

target_include_directories(xnet PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/port/fake
)

target_link_libraries(xnet PUBLIC xsdk::xutil)

xsdk_apply_target_warnings(xnet)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
