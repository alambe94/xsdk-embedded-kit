if(NOT xSDK_SOC_AM243X)
    return()
endif()

enable_language(ASM)

set(TI_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../third_party/ti/mcupsdk_core")
set(TI_SRC "${TI_SDK}/source")

set(XBOOT_LINKER_CMD "${CMAKE_CURRENT_SOURCE_DIR}/linker_am243x.cmd")

add_executable(xboot_am243x
    xboot_startup_am243x.S
    xboot_port_am243x.c
)

target_include_directories(xboot_am243x PRIVATE
    ${TI_SRC}
    ${TI_SRC}/drivers/hw_include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../drivers/xuart/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../port/am243x
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../port/am243x/sbl
)

target_compile_definitions(xboot_am243x PRIVATE SOC_AM243X OS_NORTOS)
target_compile_options(xboot_am243x PRIVATE -Wno-unused-function -Wno-unused-parameter -Wno-pedantic)

target_link_libraries(xboot_am243x PRIVATE
    xboot
    ti_sbl_support
    xsdk::xuart
)

target_link_options(xboot_am243x PRIVATE
    "${XBOOT_LINKER_CMD}"
    -Wl,--diag_suppress=10063
    -Wl,--ram_model
    -Wl,--reread_libs
    -Wl,-m,${CMAKE_CURRENT_BINARY_DIR}/xboot_am243x.map
)

set_target_properties(xboot_am243x PROPERTIES
    OUTPUT_NAME "xboot_am243x"
    SUFFIX ".out"
)

# Post-build step to generate xboot.hs_fs.tiimage
find_program(PYTHON_EXE NAMES python python3 py)

set(TIFS_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../third_party/ti/tifs_mcu_common")
set(SIGNING_DIR   "${TIFS_DIR}/tools/boot/signing")
set(ROM_IMAGE_GEN "${SIGNING_DIR}/rom_image_gen.py")
set(ROM_DEGEN_KEY "${SIGNING_DIR}/rom_degenerateKey.pem")

set(SYSFW_DIR     "${TI_SRC}/drivers/sciclient/soc/am64x_am243x")
set(SYSFW_BIN     "${SYSFW_DIR}/sysfw-hs-fs-enc.bin")
set(SYSFW_CERT    "${SYSFW_DIR}/sysfw-hs-fs-enc-cert.bin")
set(BOARDCFG_BLOB
    "${TI_SRC}/drivers/sciclient/sciclient_default_boardcfg/am243x/boardcfg_blob.bin")

set(XBOOT_OUT  "${CMAKE_CURRENT_BINARY_DIR}/xboot_am243x.out")
set(XBOOT_BIN  "${CMAKE_CURRENT_BINARY_DIR}/xboot_am243x.bin")
set(TIIMAGE    "${CMAKE_CURRENT_BINARY_DIR}/xboot.hs_fs.tiimage")

if(PYTHON_EXE AND EXISTS "${ROM_IMAGE_GEN}")
    add_custom_command(TARGET xboot_am243x POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --strip-all -O binary "${XBOOT_OUT}" "${XBOOT_BIN}"
        COMMAND ${PYTHON_EXE} "${ROM_IMAGE_GEN}"
            --swrv 1
            --sbl-bin "${XBOOT_BIN}"
            --sysfw-bin "${SYSFW_BIN}"
            --sysfw-inner-cert "${SYSFW_CERT}"
            --boardcfg-blob "${BOARDCFG_BLOB}"
            --sbl-loadaddr 0x70000000
            --sysfw-loadaddr 0x44000
            --bcfg-loadaddr 0x7B000
            --key "${ROM_DEGEN_KEY}"
            --rom-image "${TIIMAGE}"
        BYPRODUCTS "${XBOOT_BIN}" "${TIIMAGE}"
        COMMENT "Generating xboot.hs_fs.tiimage"
        VERBATIM
    )
    message(STATUS "xBOOT tiimage post-build step configured")
else()
    message(STATUS "xBOOT tiimage post-build: signing scripts not found - .out only")
endif()
