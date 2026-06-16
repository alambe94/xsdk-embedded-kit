add_library(xbridge STATIC
    src/xbridge_core.c
    src/xbridge_uart.c
    src/xbridge_i2c.c
    src/xbridge_spi.c
    src/xbridge_can.c
    src/xbridge_qspi.c
    src/xbridge_dap.c
    src/xbridge_gpio.c
    src/xbridge_pwm.c
    src/xbridge_adc.c
)

add_library(xsdk::xbridge ALIAS xbridge)

target_include_directories(xbridge PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xbridge PUBLIC xsdk::xutil)

xsdk_apply_target_warnings(xbridge)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
