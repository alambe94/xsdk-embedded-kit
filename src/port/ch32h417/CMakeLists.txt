if(NOT xSDK_SOC_CH32H417)
    return()
endif()

enable_language(ASM)

# Define the port library containing board runtime drivers
add_library(xsdk_port_ch32h417 STATIC
    xsdk_soc_runtime.c
    xrcc_drv.c
    xsdk_port_ch32h417.c
    xpfic_drv.c
    xsystick_drv.c
)
add_library(xsdk::port::ch32h417 ALIAS xsdk_port_ch32h417)
target_link_libraries(xsdk_port_ch32h417 PUBLIC xsdk::xgpio)

target_include_directories(xsdk_port_ch32h417
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wch/ch32h417/EVT/EXAM/SRC/Peripheral/inc
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/wch/ch32h417/EVT/EXAM/SRC/Core
)

option(xSDK_CH32H417_RAM "Build for running from RAM" OFF)

if(xSDK_CH32H417_RAM)
    # Expose RAM startup and linker script paths for application targets.
    set(XSDK_CH32H417_STARTUP
        "${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_v5f.S"
        "${CMAKE_CURRENT_SOURCE_DIR}/startup/ram_entry_v5f.S"
        CACHE STRING "CH32H417 startup files" FORCE
    )
    set(XSDK_CH32H417_LINKER_CMD
        "${CMAKE_CURRENT_SOURCE_DIR}/linkers/linker_v5f_ram.ld"
        CACHE PATH "CH32H417 linker script file" FORCE
    )
    add_compile_definitions(xSDK_CH32H417_RAM=1)
else()
    # Expose startup and linker script paths for application targets.
    set(XSDK_CH32H417_STARTUP
        "${CMAKE_CURRENT_SOURCE_DIR}/startup/startup_v5f.S"
        CACHE STRING "CH32H417 startup files" FORCE
    )
    set(XSDK_CH32H417_LINKER_CMD
        "${CMAKE_CURRENT_SOURCE_DIR}/linkers/linker_v5f.ld"
        CACHE PATH "CH32H417 linker script file" FORCE
    )
endif()
