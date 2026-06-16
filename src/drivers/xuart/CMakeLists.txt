add_library(xuart STATIC
    src/xuart.c
)

add_library(xsdk::xuart ALIAS xuart)

target_include_directories(xuart
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xuart PUBLIC xsdk::xutil xsdk::xtrace)

if(xSDK_SOC_AM243X)
    target_sources(xuart PRIVATE
        port/hil/am243x/xuart_port_am243x.c
    )
    target_include_directories(xuart PRIVATE
        port/hil/am243x
    )
endif()

if(xSDK_SOC_CH32H417)
    target_sources(xuart PRIVATE
        port/hil/ch32h417/xuart_drv.c
    )
    target_include_directories(xuart PRIVATE
        port/hil/ch32h417
        ${PROJECT_SOURCE_DIR}/src/port/ch32h417
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Peripheral/inc
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Core
    )
endif()

xsdk_apply_target_warnings(xuart)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
