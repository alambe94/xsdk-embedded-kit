add_library(xgpio STATIC
    src/xgpio.c
)

add_library(xsdk::xgpio ALIAS xgpio)

target_include_directories(xgpio
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xgpio PUBLIC xsdk::xutil xsdk::xtrace)

if(xSDK_SOC_CH32H417)
    target_sources(xgpio PRIVATE
        port/hil/ch32h417/xgpio_drv.c
    )
    target_include_directories(xgpio PUBLIC
        port/hil/ch32h417
    )
    target_include_directories(xgpio PRIVATE
        ${PROJECT_SOURCE_DIR}/src/port/ch32h417
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Peripheral/inc
        ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Core
    )
endif()

xsdk_apply_target_warnings(xgpio)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
