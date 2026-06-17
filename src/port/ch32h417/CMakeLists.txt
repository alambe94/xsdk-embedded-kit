if(NOT xSDK_SOC_CH32H417)
    return()
endif()

enable_language(ASM)

# Define the port library containing board runtime drivers
add_library(xsdk_port_ch32h417 STATIC
    xsdk_soc_runtime.c
    xrcc_drv.c
    xgpio_drv.c
    xsdk_port_ch32h417.c
    xpfic_drv.c
    xsystick_drv.c
)
add_library(xsdk::port::ch32h417 ALIAS xsdk_port_ch32h417)

target_include_directories(xsdk_port_ch32h417
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wch/ch32h417/EVT/EXAM/SRC/Peripheral/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wch/ch32h417/EVT/EXAM/SRC/Core
)

# Expose startup and linker script paths for application targets.
set(XSDK_CH32H417_STARTUP
    "${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_v5f.S"
    CACHE PATH "CH32H417 startup file" FORCE
)
set(XSDK_CH32H417_LINKER_CMD
    "${CMAKE_CURRENT_SOURCE_DIR}/linkers/linker_v5f.ld"
    CACHE PATH "CH32H417 linker script file" FORCE
)
