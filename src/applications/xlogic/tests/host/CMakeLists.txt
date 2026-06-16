# xLOGIC host tests - no hardware required.
# Both test executables link against xlogic and unity; no fff needed
# because the PRU and transport are faked via static arrays and inline helpers
# in test_helpers.h.

function(xlogic_add_test stem)
    xsdk_add_test(xlogic_${stem}_test
        SOURCES  test_xlogic_${stem}.c
        LIBS     xlogic::xlogic unity
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}
        ${ARGN}
    )
endfunction()

xlogic_add_test(sump)
xlogic_add_test(core)
