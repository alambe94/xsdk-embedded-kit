if(NOT xSDK_SOC_AM243X)
    return()
endif()

enable_language(ASM)

# SoC init layer - C sources only.
# startup_am243x.S is linked directly into each application target.
# Device drivers (xuart, xtimer, etc.) live under src/drivers/.
add_library(xsdk_port_am243x STATIC
    xsdk_port_am243x.c
)
add_library(xsdk::port::am243x ALIAS xsdk_port_am243x)

target_include_directories(xsdk_port_am243x
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(xsdk_port_am243x PRIVATE xsdk::xrtos)

# Expose startup and linker script paths for application targets.
set(XSDK_AM243X_STARTUP
    "${CMAKE_CURRENT_SOURCE_DIR}/startup_am243x.S"
    CACHE PATH "AM243x startup file" FORCE
)
set(XSDK_AM243X_LINKER_CMD
    "${CMAKE_CURRENT_SOURCE_DIR}/linker.cmd"
    CACHE PATH "AM243x linker command file" FORCE
)

add_subdirectory(sbl)
