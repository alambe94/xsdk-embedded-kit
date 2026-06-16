include_guard(GLOBAL)

# xsdk_add_qemu_test() - register an ARM R5 QEMU smoke-test executable.
#
# Each component defines its own QEMU test target by calling this function
# from its own CMakeLists.txt. The shared startup.S, linker.ld, and uart.h
# are in the same directory as this file (src/port/qemu_r5/).
#
# Usage:
#   xsdk_add_qemu_test(
#       TARGET       <cmake-target-name>
#       SOURCES      <file> [<file>...]   # test-specific .c and .S sources
#       LIBS         <lib> [<lib>...]     # libraries to link
#       BOARD        REALVIEW_PB_A8 | ZYNQ_A9
#       OUTPUT       <elf-basename>       # output filename without .elf
#       [INCLUDE_DIRS <dir> [<dir>...]]   # extra private include paths
#       [PASS_STRING  <string>]           # UART substring that signals PASS
#       [IS_TRACE]                        # target captures UART1 trace output
#   )
#
# CTest integration:
#   When PASS_STRING is provided and both QEMU_ARM_EXE and PYTHON_EXECUTABLE
#   are found, add_test() is called automatically - no xsdk.bat changes needed
#   when adding new tests.  Override the discovered paths at configure time:
#     -DQEMU_ARM_EXE=/path/to/qemu-system-arm
#     -DPYTHON_EXECUTABLE=/path/to/python3
#
# Guards: silently returns when:
#   - CMAKE_SYSTEM_PROCESSOR is not "arm"
#   - CMAKE_C_COMPILER_ID is not "GNU"

set(_QEMU_PORT_DIR "${CMAKE_SOURCE_DIR}/src/port/qemu_r5")

find_program(QEMU_ARM_EXE
    NAMES qemu-system-arm qemu-system-arm.exe
    HINTS "${CMAKE_SOURCE_DIR}/tools/qemu/bin"
    NO_CMAKE_FIND_ROOT_PATH
)
mark_as_advanced(QEMU_ARM_EXE)

find_program(PYTHON_EXECUTABLE
    NAMES python3 python python3.exe python.exe
    NO_CMAKE_FIND_ROOT_PATH
)
mark_as_advanced(PYTHON_EXECUTABLE)

function(xsdk_add_qemu_test)
    cmake_parse_arguments(
        ARG
        "IS_TRACE"
        "TARGET;BOARD;OUTPUT;PASS_STRING"
        "SOURCES;LIBS;INCLUDE_DIRS;DEFS"
        ${ARGN}
    )

    if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm")
        return()
    endif()
    if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        return()
    endif()

    set(_PORT_DIR "${_QEMU_PORT_DIR}")

    add_executable(${ARG_TARGET}
        "${_PORT_DIR}/startup.S"
        ${ARG_SOURCES}
    )

    target_compile_definitions(${ARG_TARGET} PRIVATE "BOARD_${ARG_BOARD}" ${ARG_DEFS})

    target_include_directories(${ARG_TARGET} PRIVATE
        "${_PORT_DIR}"
        ${ARG_INCLUDE_DIRS}
    )

    target_link_libraries(${ARG_TARGET} PRIVATE ${ARG_LIBS})

    target_link_options(${ARG_TARGET} PRIVATE
        -T "${_PORT_DIR}/linker.ld"
        -nostartfiles
        --specs=nosys.specs
    )

    set_target_properties(${ARG_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
        OUTPUT_NAME "${ARG_OUTPUT}.elf"
    )

    xsdk_apply_target_warnings(${ARG_TARGET})

    if(ARG_PASS_STRING AND QEMU_ARM_EXE AND PYTHON_EXECUTABLE)
        set(_runner "${CMAKE_SOURCE_DIR}/tools/qemu/run.py")
        set(_cmd
            "${PYTHON_EXECUTABLE}" "${_runner}"
            --qemu "${QEMU_ARM_EXE}"
            --kernel "$<TARGET_FILE:${ARG_TARGET}>"
            --pass-string "${ARG_PASS_STRING}"
            --timeout 15
        )
        if(ARG_IS_TRACE)
            set(_trace_dir "${CMAKE_BINARY_DIR}/xtrace_out")
            file(MAKE_DIRECTORY "${_trace_dir}")
            list(APPEND _cmd --serial-file "${_trace_dir}/${ARG_TARGET}_trace.bin")
        endif()

        add_test(NAME "${ARG_TARGET}" COMMAND ${_cmd})

        if(ARG_IS_TRACE)
            set_tests_properties("${ARG_TARGET}" PROPERTIES TIMEOUT 20 LABELS "qemu;trace")
        else()
            set_tests_properties("${ARG_TARGET}" PROPERTIES TIMEOUT 20 LABELS "qemu")
        endif()
    endif()
endfunction()
