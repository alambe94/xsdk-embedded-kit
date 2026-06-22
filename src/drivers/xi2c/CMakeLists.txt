add_library(xi2c STATIC
    src/xi2c.c
)

add_library(xsdk::xi2c ALIAS xi2c)

target_include_directories(xi2c
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xi2c PUBLIC xsdk::xutil xsdk::xtrace)

if(xSDK_SOC_AM243X)
    target_sources(xi2c PRIVATE
        port/hil/am243x/xi2c_drv.c
    )
    target_include_directories(xi2c PUBLIC
        port/hil/am243x
    )
    target_include_directories(xi2c PRIVATE
        ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source
        ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source/drivers/hw_include
    )
    target_compile_definitions(xi2c PRIVATE SOC_AM243X)
endif()

if(xSDK_SOC_CH32H417)
    target_sources(xi2c PRIVATE
        port/hil/ch32h417/xi2c_drv.c
    )
    target_include_directories(xi2c PUBLIC
        port/hil/ch32h417
    )
    target_include_directories(xi2c PRIVATE
        ${PROJECT_SOURCE_DIR}/src/port/ch32h417
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Peripheral/inc
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Core
    )
endif()

xsdk_apply_target_warnings(xi2c)
