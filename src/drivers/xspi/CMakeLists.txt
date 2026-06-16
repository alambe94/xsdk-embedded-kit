add_library(xspi STATIC
    src/xspi.c
)

add_library(xsdk::xspi ALIAS xspi)

target_include_directories(xspi
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xspi PUBLIC xsdk::xutil)

if(xSDK_SOC_AM243X)
    target_sources(xspi PRIVATE
        port/hil/am243x/xspi_port_am243x.c
    )
    target_include_directories(xspi PRIVATE
        port/hil/am243x
        ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source
        ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source/drivers/hw_include
    )
    target_compile_definitions(xspi PRIVATE SOC_AM243X)
endif()

xsdk_apply_target_warnings(xspi)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
