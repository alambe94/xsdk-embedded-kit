if(NOT xSDK_SOC_AM243X)
    return()
endif()

add_executable(am243x_rr32
    main.c
    ${XSDK_AM243X_STARTUP}
)

target_link_libraries(am243x_rr32 PRIVATE
    xsdk::xrtos
    xsdk::port::am243x
    xsdk::xuart
    xsdk::xtimer
)

target_link_options(am243x_rr32 PRIVATE
    "${XSDK_AM243X_LINKER_CMD}"
    -Wl,-m,${CMAKE_CURRENT_BINARY_DIR}/am243x_rr32.map
)

set_target_properties(am243x_rr32 PROPERTIES
    OUTPUT_NAME "am243x_rr32"
    SUFFIX ".out"
)
