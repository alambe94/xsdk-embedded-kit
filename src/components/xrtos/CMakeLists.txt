add_library(xrtos STATIC
    src/xrtos_core.c
    src/xrtos_scheduler.c
    src/xrtos_tick.c
    src/xrtos_notify.c
    src/xrtos_sem.c
    src/xrtos_mutex.c
    src/xrtos_event.c
    src/xrtos_queue.c
    src/xrtos_timer.c
)

add_library(xsdk::xrtos ALIAS xrtos)

target_include_directories(xrtos
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(xrtos PUBLIC xsdk::xutil xsdk::xtrace)

if(xSDK_SOC_CH32H417)
    enable_language(ASM)
    target_sources(xrtos PRIVATE
        port/hil/ch32h417/xrtos_port_ch32h417.c
        port/hil/ch32h417/xrtos_port_ch32h417_context.S
    )
    target_include_directories(xrtos PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/port/hil/ch32h417
    )
    target_compile_definitions(xrtos PUBLIC
        xRTOS_CONFIG_TIMER_METHOD=0
        xRTOS_CONFIG_MAX_TASKS=8U
        xRTOS_CONFIG_MAX_PRIORITIES=8U
        xRTOS_CONFIG_MAX_TIMERS=4U
    )
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
    enable_language(ASM)

    # Common Cortex-R5 sources
    target_sources(xrtos PRIVATE
        port/arm_r5/xrtos_port_arm_r5.c
        port/arm_r5/xrtos_port_arm_r5_asm.S
    )
    target_include_directories(xrtos PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/port/arm_r5
    )

    if(xSDK_SOC_AM243X)
        target_sources(xrtos PRIVATE
            port/hil/am243x/xrtos_port_am243x.c
            port/hil/am243x/xrtos_port_am243x_vectors.S
        )
        target_include_directories(xrtos PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/port/hil/am243x
        )
        target_compile_definitions(xrtos PRIVATE XRTOS_CONFIG_DISABLE_FIQ=1)
    else()
        target_sources(xrtos PRIVATE
            port/qemu/arm_r5/xrtos_port_qemu_r5.c
        )
        target_include_directories(xrtos PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/port/qemu/arm_r5
        )
    endif()

    # Enable round-robin for all ARM builds. Existing QEMU tests are unaffected
    # because their tasks all have unique base priorities - the tick handler only
    # triggers a round-robin switch when ready_head[current_priority] is non-NULL,
    # which only happens when two or more tasks share the same effective priority.
    target_compile_definitions(xrtos PRIVATE xRTOS_CONFIG_ROUND_ROBIN_ENABLE=1)

    xsdk_add_qemu_test(TARGET xrtos_qemu
        SOURCES tests/qemu/main.c
        LIBS    xrtos
        BOARD   ZYNQ_A9
        OUTPUT  xrtos_qemu)

    xsdk_add_qemu_test(TARGET xrtos_qemu_realview
        SOURCES     tests/qemu/main.c
        LIBS        xrtos
        BOARD       REALVIEW_PB_A8
        OUTPUT      xrtos_qemu_realview
        PASS_STRING "ALL PHASES COMPLETE")

    xsdk_add_qemu_test(TARGET xrtos_qemu_realview_trace
        SOURCES     tests/qemu/main.c
        LIBS        xrtos xsdk::xtrace
        BOARD       REALVIEW_PB_A8
        DEFS        xTRACE_ENABLE=1 QEMU_TRACE_ENABLED
        OUTPUT      xrtos_qemu_realview_trace
        PASS_STRING "ALL PHASES COMPLETE"
        IS_TRACE)

    xsdk_add_qemu_test(TARGET xrtos_qemu_rr32_realview
        SOURCES     tests/qemu/test_rr32.c
        LIBS        xrtos
        BOARD       REALVIEW_PB_A8
        OUTPUT      xrtos_qemu_rr32_realview
        PASS_STRING "RR32 PASS")

    xsdk_add_qemu_test(TARGET xrtos_qemu_rr32_realview_trace
        SOURCES     tests/qemu/test_rr32.c
        LIBS        xrtos xsdk::xtrace
        BOARD       REALVIEW_PB_A8
        DEFS        xTRACE_ENABLE=1 QEMU_TRACE_ENABLED
        OUTPUT      xrtos_qemu_rr32_realview_trace
        PASS_STRING "RR32 PASS"
        IS_TRACE)

    xsdk_add_qemu_test(TARGET xrtos_qemu_showcase_realview
        SOURCES     tests/qemu/test_perfetto_showcase.c
        LIBS        xrtos
        BOARD       REALVIEW_PB_A8
        OUTPUT      xrtos_qemu_showcase_realview
        PASS_STRING "PERF_SHOW PASS")

    xsdk_add_qemu_test(TARGET xrtos_qemu_showcase_realview_trace
        SOURCES     tests/qemu/test_perfetto_showcase.c
        LIBS        xrtos xsdk::xtrace
        BOARD       REALVIEW_PB_A8
        DEFS        xTRACE_ENABLE=1 QEMU_TRACE_ENABLED
        OUTPUT      xrtos_qemu_showcase_realview_trace
        PASS_STRING "PERF_SHOW PASS"
        IS_TRACE)

    xsdk_add_qemu_test(TARGET xrtos_qemu_regression_realview
        SOURCES     tests/qemu/test_xrtos_regression.c
        LIBS        xrtos
        BOARD       REALVIEW_PB_A8
        OUTPUT      xrtos_qemu_regression_realview
        PASS_STRING "REGRESSION PASS")
endif()

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
