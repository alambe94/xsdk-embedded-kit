if(NOT xSDK_SOC_CH32H417)
    return()
endif()

add_executable(ch32h417_bringup
    main.c
    ${XSDK_CH32H417_STARTUP}
)

target_link_libraries(ch32h417_bringup PRIVATE
    xsdk::xrtos
    xsdk::xusbd
    xsdk::xuart
    xsdk::xi2c
    xsdk::xspi
    xsdk::xtimer
    xsdk::port::ch32h417
)

target_link_options(ch32h417_bringup PRIVATE
    -nostartfiles
    -nostdlib
    "-T${XSDK_CH32H417_LINKER_CMD}"
    -Wl,--gc-sections
    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/ch32h417_bringup.map"
)

xsdk_apply_target_warnings(ch32h417_bringup)

set_target_properties(ch32h417_bringup PROPERTIES
    OUTPUT_NAME "ch32h417_bringup"
    SUFFIX ".elf"
)

add_custom_command(TARGET ch32h417_bringup POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:ch32h417_bringup>
            ${CMAKE_CURRENT_BINARY_DIR}/ch32h417_bringup.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:ch32h417_bringup>
            ${CMAKE_CURRENT_BINARY_DIR}/ch32h417_bringup.bin
    COMMENT "Generating CH32H417 HEX and BIN images"
    VERBATIM
)
