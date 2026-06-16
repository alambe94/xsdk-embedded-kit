if(NOT xSDK_SOC_AM243X)
    return()
endif()

set(TI_SDK "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/ti/mcupsdk_core")
set(TI_SRC "${TI_SDK}/source")

enable_language(ASM)

# ---- source file lists -------------------------------------------------------

set(SBL_NULL_NORTOS_C
    ${TI_SRC}/kernel/nortos/dpl/r5/boot_armv7r.c
    ${TI_SRC}/kernel/nortos/dpl/r5/CacheP_armv7r.c
    ${TI_SRC}/kernel/nortos/dpl/r5/ClockP_nortos_r5.c
    ${TI_SRC}/kernel/nortos/dpl/r5/CpuId_armv7r.c
    ${TI_SRC}/kernel/nortos/dpl/r5/HwiP_armv7r_handlers_nortos.c
    ${TI_SRC}/kernel/nortos/dpl/r5/HwiP_armv7r_vim.c
    ${TI_SRC}/kernel/nortos/dpl/r5/MpuP_armv7r.c
    ${TI_SRC}/kernel/nortos/dpl/r5/PmuP_armv7r.c
    ${TI_SRC}/kernel/nortos/dpl/common/AddrTranslateP_null.c
    ${TI_SRC}/kernel/nortos/dpl/common/ClockP_nortos.c
    ${TI_SRC}/kernel/nortos/dpl/common/DebugP_log.c
    ${TI_SRC}/kernel/nortos/dpl/common/DebugP_nortos.c
    ${TI_SRC}/kernel/nortos/dpl/common/HeapP_nortos.c
    ${TI_SRC}/kernel/nortos/dpl/common/SemaphoreP_nortos.c
    ${TI_SRC}/kernel/nortos/dpl/common/TaskP_nortos.c
    ${TI_SRC}/kernel/nortos/dpl/common/TimerP.c
    ${TI_SRC}/kernel/nortos/dpl/common/printf.c
)

set(SBL_NULL_NORTOS_ASM
    ${TI_SRC}/kernel/nortos/dpl/r5/boot_armv7r_asm.S
    ${TI_SRC}/kernel/nortos/dpl/r5/CacheP_armv7r_asm.S
    ${TI_SRC}/kernel/nortos/dpl/r5/CpuId_armv7r_asm.S
    ${TI_SRC}/kernel/nortos/dpl/r5/HwiP_armv7r_asm.S
    ${TI_SRC}/kernel/nortos/dpl/r5/HwiP_armv7r_handlers_nortos_asm.S
    ${TI_SRC}/kernel/nortos/dpl/r5/HwiP_armv7r_vectors_nortos_sbl_asm.S
    ${TI_SRC}/kernel/nortos/dpl/r5/MpuP_armv7r_asm.S
    ${TI_SRC}/kernel/nortos/dpl/r5/Mutex_armv7r_asm.S
    ${TI_SRC}/kernel/nortos/dpl/r5/PmuP_armv7r_asm.S
)

set(SBL_NULL_BOOTLOADER_C
    ${TI_SRC}/drivers/bootloader/bootloader.c
    ${TI_SRC}/drivers/bootloader/bootloader_mem.c
    ${TI_SRC}/drivers/bootloader/soc/am64x_am243x/bootloader_soc.c
)

set(SBL_NULL_SCICLIENT_C
    ${TI_SRC}/drivers/sciclient/sciclient.c
    ${TI_SRC}/drivers/sciclient/sciclient_pm.c
    ${TI_SRC}/drivers/sciclient/sciclient_rm.c
    ${TI_SRC}/drivers/sciclient/sciclient_rm_irq.c
    ${TI_SRC}/drivers/sciclient/sciclient_procboot.c
    ${TI_SRC}/drivers/sciclient/sciclient_boardcfg.c
    ${TI_SRC}/drivers/sciclient/sciclient_firewall.c
    ${TI_SRC}/drivers/sciclient/csl_sec_proxy.c
    ${TI_SRC}/drivers/sciclient/soc/am64x_am243x/sciclient_fmwSecureProxyMap.c
    ${TI_SRC}/drivers/sciclient/soc/am64x_am243x/sciclient_irq_rm.c
    ${TI_SRC}/drivers/sciclient/soc/am64x_am243x/sciclient_soc_priv.c
)

set(SBL_NULL_SOC_C
    ${TI_SRC}/drivers/soc/am64x_am243x/soc.c
)

set(SBL_NULL_APP_C
    ${TI_SDK}/examples/drivers/boot/sbl_null/am243x-lp/r5fss0-0_nortos/main.c
)

set(SBL_NULL_XSDK_C
    ${CMAKE_CURRENT_SOURCE_DIR}/../xsdk_sbl_board_init.c
)

# ---- intermediate static library for TI SDK sources -------------------------
# The TI linker's RSP file parser treats '-' as a flag prefix, so paths
# containing "mcupsdk-core" break when passed as individual .obj arguments.
# Collecting TI sources into a .a hides the deep hyphenated paths; the
# executable RSP only sees the short "libti_sbl_support.a" path.

add_library(ti_sbl_support STATIC
    ${SBL_NULL_NORTOS_C}
    ${SBL_NULL_NORTOS_ASM}
    ${SBL_NULL_BOOTLOADER_C}
    ${SBL_NULL_SCICLIENT_C}
    ${SBL_NULL_SOC_C}
)

target_include_directories(ti_sbl_support PRIVATE
    ${TI_SRC}
    ${TI_SRC}/drivers/hw_include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../drivers/xuart/include
)

target_compile_definitions(ti_sbl_support PRIVATE SOC_AM243X OS_NORTOS)
target_compile_options(ti_sbl_support PRIVATE -Wno-unused-function -Wno-unused-parameter -Wno-pedantic)

# ---- executable --------------------------------------------------------------

set(SBL_LINKER_CMD "${CMAKE_CURRENT_SOURCE_DIR}/linker_sbl_null.cmd")

add_executable(am243x_sbl_null
    ${SBL_NULL_APP_C}
    ${SBL_NULL_XSDK_C}
)

target_include_directories(am243x_sbl_null PRIVATE
    ${TI_SRC}
    ${TI_SRC}/drivers/hw_include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../drivers/xuart/include
)

target_compile_definitions(am243x_sbl_null PRIVATE SOC_AM243X OS_NORTOS)
target_compile_options(am243x_sbl_null PRIVATE -Wno-unused-function -Wno-unused-parameter -Wno-pedantic)

target_link_libraries(am243x_sbl_null PRIVATE
    ti_sbl_support
    xsdk::xuart
)

target_link_options(am243x_sbl_null PRIVATE
    "${SBL_LINKER_CMD}"
    -Wl,--diag_suppress=10063
    -Wl,--ram_model
    -Wl,--reread_libs
    -Wl,-m,${CMAKE_CURRENT_BINARY_DIR}/am243x_sbl_null.map
)

set_target_properties(am243x_sbl_null PROPERTIES
    OUTPUT_NAME "am243x_sbl_null"
    SUFFIX ".out"
)

# ---- post-build: produce sbl_null.release.hs_fs.tiimage -------------------
# Signing scripts from tifs_mcu_common submodule.
# Commit: b237ec912feef47a2566f29dac14f5d95057d868 (REL.MCUSDK.K3.11.02.00.24)

find_program(PYTHON_EXE NAMES python python3 py)

set(TIFS_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/ti/tifs_mcu_common")
set(SIGNING_DIR   "${TIFS_DIR}/tools/boot/signing")
set(ROM_IMAGE_GEN "${SIGNING_DIR}/rom_image_gen.py")
set(ROM_DEGEN_KEY "${SIGNING_DIR}/rom_degenerateKey.pem")

set(SYSFW_DIR     "${TI_SRC}/drivers/sciclient/soc/am64x_am243x")
set(SYSFW_BIN     "${SYSFW_DIR}/sysfw-hs-fs-enc.bin")
set(SYSFW_CERT    "${SYSFW_DIR}/sysfw-hs-fs-enc-cert.bin")
set(BOARDCFG_BLOB
    "${TI_SRC}/drivers/sciclient/sciclient_default_boardcfg/am243x/boardcfg_blob.bin")

set(SBL_OUT  "${CMAKE_CURRENT_BINARY_DIR}/am243x_sbl_null.out")
set(SBL_BIN  "${CMAKE_CURRENT_BINARY_DIR}/am243x_sbl_null.bin")
set(TIIMAGE  "${CMAKE_CURRENT_BINARY_DIR}/sbl_null.release.hs_fs.tiimage")

if(PYTHON_EXE AND EXISTS "${ROM_IMAGE_GEN}")
    add_custom_command(TARGET am243x_sbl_null POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --strip-all -O binary "${SBL_OUT}" "${SBL_BIN}"
        COMMAND ${PYTHON_EXE} "${ROM_IMAGE_GEN}"
            --swrv 1
            --sbl-bin "${SBL_BIN}"
            --sysfw-bin "${SYSFW_BIN}"
            --sysfw-inner-cert "${SYSFW_CERT}"
            --boardcfg-blob "${BOARDCFG_BLOB}"
            --sbl-loadaddr 0x70000000
            --sysfw-loadaddr 0x44000
            --bcfg-loadaddr 0x7B000
            --key "${ROM_DEGEN_KEY}"
            --rom-image "${TIIMAGE}"
        BYPRODUCTS "${SBL_BIN}" "${TIIMAGE}"
        COMMENT "Generating sbl_null.release.hs_fs.tiimage"
        VERBATIM
    )
    message(STATUS "SBL tiimage post-build step configured")
else()
    message(STATUS "SBL tiimage post-build: signing scripts not found - .out only")
endif()
