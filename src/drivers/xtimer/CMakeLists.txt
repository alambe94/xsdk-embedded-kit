if(NOT xSDK_SOC_AM243X AND NOT xSDK_SOC_CH32H417)
    return()
endif()

add_library(xtimer STATIC)

add_library(xsdk::xtimer ALIAS xtimer)

target_include_directories(xtimer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(xSDK_SOC_AM243X)
    target_sources(xtimer PRIVATE
        port/hil/am243x/xtimer_drv.c
    )
endif()

if(xSDK_SOC_CH32H417)
    target_sources(xtimer PRIVATE
        port/hil/ch32h417/xtimer_drv.c
    )
    target_include_directories(xtimer PRIVATE
        ${PROJECT_SOURCE_DIR}/src/port/ch32h417
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Peripheral/inc
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Core
    )
endif()
