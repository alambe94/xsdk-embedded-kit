# xBRIDGE host unit tests - one Unity main() executable per channel.
#
# test_helpers.h is found via INCLUDES; fff is linked directly because
# DEFINE_FFF_GLOBALS lives in test_helpers.h (single-TU per executable).

function(xbridge_add_test stem)
    xsdk_add_test(xbridge_${stem}_test
        SOURCES  test_xbridge_${stem}.c
        LIBS     xsdk::xbridge fff
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARGN}
    )
endfunction()

xbridge_add_test(uart)
xbridge_add_test(i2c)
xbridge_add_test(spi)
xbridge_add_test(can_slcan)
xbridge_add_test(dap)
xbridge_add_test(gpio)
xbridge_add_test(pwm)
xbridge_add_test(adc)
