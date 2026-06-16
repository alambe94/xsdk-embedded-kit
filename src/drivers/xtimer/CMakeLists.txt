if(NOT xSDK_SOC_AM243X)
    return()
endif()

add_library(xtimer STATIC
    port/hil/am243x/xtimer_port_am243x.c
)

add_library(xsdk::xtimer ALIAS xtimer)

target_include_directories(xtimer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)
