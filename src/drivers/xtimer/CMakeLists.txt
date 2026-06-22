add_library(xtimer STATIC
    src/xtimer.c
)

add_library(xsdk::xtimer ALIAS xtimer)

target_include_directories(xtimer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xtimer PUBLIC xsdk::xutil xsdk::xtrace)

if(xSDK_SOC_AM243X)
    target_sources(xtimer PRIVATE
        port/hil/am243x/xtimer_drv.c
    )
    target_include_directories(xtimer PUBLIC
        port/hil/am243x
    )
endif()

if(xSDK_SOC_CH32H417)
    target_sources(xtimer PRIVATE
        port/hil/ch32h417/xtimer_drv.c
    )
    target_include_directories(xtimer PUBLIC
        port/hil/ch32h417
    )
    target_include_directories(xtimer PRIVATE
        ${PROJECT_SOURCE_DIR}/src/port/ch32h417
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Peripheral/inc
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Core
    )
endif()

xsdk_apply_target_warnings(xtimer)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
