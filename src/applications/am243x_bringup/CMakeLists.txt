if(NOT xSDK_SOC_AM243X)
    return()
endif()

add_executable(am243x_bringup
    main.c
    ${XSDK_AM243X_STARTUP}
)

target_link_libraries(am243x_bringup PRIVATE
    xsdk::xrtos
    xsdk::port::am243x
    xsdk::xuart
    xsdk::xtimer
)

target_link_options(am243x_bringup PRIVATE
    "${XSDK_AM243X_LINKER_CMD}"
    -Wl,-m,${CMAKE_CURRENT_BINARY_DIR}/am243x_bringup.map
)

set_target_properties(am243x_bringup PROPERTIES
    OUTPUT_NAME "am243x_bringup"
    SUFFIX ".out"
)

# ---- Phase 8: HS-FS boot image pipeline ------------------------------------
# .out -> .rprc -> .appimage -> .appimage.hs_fs
#
# AM243x image parameters (from TI reference makefiles):
#   devID   = 55   (AM243x device ID for MultiCoreImageGen)
#   coreID  =  4   (R5FSS0-0 core ID)
#   OSPI layout: SBL at 0x00000, appimage at 0x80000

set(TI_SDK  "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ti/mcupsdk_core")
set(TIFS    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/ti/tifs_mcu_common")

set(OUT2RPRC  "${TI_SDK}/tools/boot/out2rprc/out2rprc.exe")
set(MCIMG     "${TI_SDK}/tools/boot/multicoreImageGen/MulticoreImageGen.exe")
set(APPSIGN   "${TIFS}/tools/boot/signing/appimage_x509_cert_gen.py")
set(APP_KEY   "${TIFS}/tools/boot/signing/app_degenerateKey.pem")

find_program(PYTHON_EXE NAMES python python3 py)

set(APP_OUT    "${CMAKE_CURRENT_BINARY_DIR}/am243x_bringup.out")
set(APP_RPRC   "${CMAKE_CURRENT_BINARY_DIR}/am243x_bringup.rprc")
set(APP_IMG    "${CMAKE_CURRENT_BINARY_DIR}/am243x_bringup.appimage")
set(APP_SIGNED "${CMAKE_CURRENT_BINARY_DIR}/am243x_bringup.appimage.hs_fs")

if(EXISTS "${OUT2RPRC}" AND EXISTS "${MCIMG}" AND PYTHON_EXE AND EXISTS "${APPSIGN}")
    add_custom_command(TARGET am243x_bringup POST_BUILD
        COMMAND "${OUT2RPRC}" "${APP_OUT}" "${APP_RPRC}"
        COMMAND "${MCIMG}" LE 55 "${APP_IMG}" 4 "${APP_RPRC}"
        COMMAND ${PYTHON_EXE} "${APPSIGN}"
            --bin "${APP_IMG}"
            --key "${APP_KEY}"
            --output "${APP_SIGNED}"
            --enc n
        BYPRODUCTS "${APP_RPRC}" "${APP_IMG}" "${APP_SIGNED}"
        COMMENT "Generating am243x_bringup.appimage.hs_fs"
        VERBATIM
    )
    message(STATUS "AM243x app boot-image pipeline configured")
else()
    message(STATUS "AM243x app boot-image: tools not found - .out only")
endif()
