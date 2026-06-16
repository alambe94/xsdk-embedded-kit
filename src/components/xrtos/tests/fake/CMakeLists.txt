# Define test-specific library targets for both ISR and Task timer modes.
# This prevents ODR/struct layout mismatches and registry pollution in standard tests.
add_library(xrtos_test_isr STATIC
    ../../src/xrtos_core.c
    ../../src/xrtos_scheduler.c
    ../../src/xrtos_tick.c
    ../../src/xrtos_notify.c
    ../../src/xrtos_sem.c
    ../../src/xrtos_mutex.c
    ../../src/xrtos_event.c
    ../../src/xrtos_queue.c
    ../../src/xrtos_timer.c
)
target_include_directories(xrtos_test_isr PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)
target_link_libraries(xrtos_test_isr PUBLIC xsdk::xutil xsdk::xtrace)
target_compile_definitions(xrtos_test_isr PUBLIC xRTOS_CONFIG_TIMER_METHOD=0U)

add_library(xrtos_test_task STATIC
    ../../src/xrtos_core.c
    ../../src/xrtos_scheduler.c
    ../../src/xrtos_tick.c
    ../../src/xrtos_notify.c
    ../../src/xrtos_sem.c
    ../../src/xrtos_mutex.c
    ../../src/xrtos_event.c
    ../../src/xrtos_queue.c
    ../../src/xrtos_timer.c
)
target_include_directories(xrtos_test_task PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)
target_link_libraries(xrtos_test_task PUBLIC xsdk::xutil xsdk::xtrace)
target_compile_definitions(xrtos_test_task PUBLIC xRTOS_CONFIG_TIMER_METHOD=1U)

# Per-module wrapper: adds the port stub source and include path shared by all
# standard xrtos tests.  stem -> executable xrtos_${stem}_test, source test_xrtos_${stem}.c
function(xsdk_add_xrtos_test stem)
    xsdk_add_test(xrtos_${stem}_test
        SOURCES  test_xrtos_${stem}.c ../../port/fake/xrtos_port_fake.c
        LIBS     xrtos_test_isr
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../port/fake
        ${ARGN}
    )
endfunction()

function(xsdk_add_xrtos_footprint_test name profile report)
    xsdk_add_test(${name}
        SOURCES  test_xrtos_footprint_report.c
        LIBS     xsdk::xutil xsdk::xtrace
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        DEFS     xRTOS_FOOTPRINT_PROFILE=\"${profile}\"
                 xRTOS_FOOTPRINT_REPORT_PATH=\"${CMAKE_CURRENT_BINARY_DIR}/${report}\"
                 ${ARGN}
    )
    set_tests_properties(${name} PROPERTIES LABELS "xrtos;footprint")
endfunction()

# Standard tests (port stub + xrtos_test_isr)
xsdk_add_xrtos_test(task)
xsdk_add_xrtos_test(task_set_priority)
xsdk_add_xrtos_test(canary)
xsdk_add_xrtos_test(scheduler)
xsdk_add_xrtos_test(trace)
xsdk_add_xrtos_test(port)
xsdk_add_xrtos_test(tick)
xsdk_add_xrtos_test(blocking)
xsdk_add_xrtos_test(isr_guard)
xsdk_add_xrtos_test(notify)
xsdk_add_xrtos_test(sem)
xsdk_add_xrtos_test(mutex)
xsdk_add_xrtos_test(event)
xsdk_add_xrtos_test(queue)
xsdk_add_xrtos_test(timer)

# Task-based timer test
xsdk_add_test(xrtos_timer_task_test
    SOURCES  test_xrtos_timer.c ../../port/fake/xrtos_port_fake.c
    LIBS     xrtos_test_task
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../port/fake
)

# Non-standard: no port stub, override config via compile defs
xsdk_add_test(xrtos_config_small_test
    SOURCES  test_xrtos_config_small.c
    LIBS     xsdk::xutil xsdk::xtrace
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    DEFS     xRTOS_CONFIG_MAX_TASKS=8U
             xRTOS_CONFIG_MAX_PRIORITIES=12U
             xRTOS_CONFIG_MAX_TIMERS=4U
)

# CPU stats enabled library (struct layout differs from xrtos_test_isr)
add_library(xrtos_test_cpu_stats_on STATIC
    ../../src/xrtos_core.c
    ../../src/xrtos_scheduler.c
    ../../src/xrtos_tick.c
    ../../src/xrtos_notify.c
    ../../src/xrtos_sem.c
    ../../src/xrtos_mutex.c
    ../../src/xrtos_event.c
    ../../src/xrtos_queue.c
    ../../src/xrtos_timer.c
)
target_include_directories(xrtos_test_cpu_stats_on PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
)
target_link_libraries(xrtos_test_cpu_stats_on PUBLIC xsdk::xutil xsdk::xtrace)
target_compile_definitions(xrtos_test_cpu_stats_on PUBLIC
    xRTOS_CONFIG_TIMER_METHOD=0U
    xRTOS_CONFIG_CPU_STATS_ENABLE=1
)

# CPU stats tests - disabled (default) and enabled builds
xsdk_add_test(xrtos_cpu_stats_disabled_test
    SOURCES  test_xrtos_cpu_stats.c ../../port/fake/xrtos_port_fake.c
    LIBS     xrtos_test_isr
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../port/fake
)

xsdk_add_test(xrtos_cpu_stats_test
    SOURCES  test_xrtos_cpu_stats.c ../../port/fake/xrtos_port_fake.c
    LIBS     xrtos_test_cpu_stats_on
    INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/../../port/fake
    DEFS     xRTOS_CONFIG_CPU_STATS_ENABLE=1
)

# Footprint report tests
xsdk_add_xrtos_footprint_test(xrtos_footprint_default_test default xrtos_footprint_default.csv)

xsdk_add_xrtos_footprint_test(xrtos_footprint_small_test small xrtos_footprint_small.csv
    xRTOS_CONFIG_MAX_TASKS=8U
    xRTOS_CONFIG_MAX_PRIORITIES=12U
    xRTOS_CONFIG_MAX_TIMERS=4U
)
