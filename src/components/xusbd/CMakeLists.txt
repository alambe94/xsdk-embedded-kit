add_library(xusbd STATIC
    src/core/xusbd_core.c
    src/core/xusbd_std.c
    src/core/xusbd_class.c
    src/class/xusbd_cdc.c
    src/class/xusbd_hid.c
    src/class/xusbd_msc.c
    src/class/xusbd_win.c
    src/class/xusbd_dfu.c
)

add_library(xsdk::xusbd ALIAS xusbd)

target_include_directories(xusbd
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/src/class
)

target_link_libraries(xusbd PUBLIC xsdk::xutil xsdk::xtrace xsdk::xusb_common)
xsdk_apply_target_warnings(xusbd)

# Port selection - set XUSBD_PORT to the MCU family or fake to include its driver.
# Example: cmake -DXUSBD_PORT=am243x
# Example: cmake -DXUSBD_PORT=fake
if(DEFINED XUSBD_PORT)
    if(XUSBD_PORT STREQUAL "fake")
        set(XUSBD_PORT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/port/fake/xusbd_drv.c")
        set(XUSBD_PORT_INC "${CMAKE_CURRENT_SOURCE_DIR}/port/fake")
    else()
        set(XUSBD_PORT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/port/hil/${XUSBD_PORT}/xusbd_drv.c")
        set(XUSBD_PORT_INC "${CMAKE_CURRENT_SOURCE_DIR}/port/hil/${XUSBD_PORT}")
    endif()

    target_sources(xusbd PRIVATE ${XUSBD_PORT_SRC})
    target_include_directories(xusbd PUBLIC ${XUSBD_PORT_INC})

    if(XUSBD_PORT STREQUAL "am243x")
        target_compile_definitions(xusbd PRIVATE
            SOC_AM243X=1
            OS_NORTOS=1
        )
        set_source_files_properties(${XUSBD_PORT_SRC} PROPERTIES COMPILE_OPTIONS "-Wno-c23-extensions")
        target_include_directories(xusbd PRIVATE
            ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source
            ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source/usb/cdn/core_driver/common/include
            ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source/usb/cdn/core_driver/common/src
            ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source/usb/cdn/core_driver/device/include
            ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source/usb/cdn/core_driver/device/src
            ${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core/source/usb/cdn/include
        )
    elseif(XUSBD_PORT STREQUAL "ch32h417")
        target_include_directories(xusbd PRIVATE
            ${PROJECT_SOURCE_DIR}/src/port/ch32h417
            ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Peripheral/inc
            ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32h417/EVT/EXAM/SRC/Core
        )
    elseif(XUSBD_PORT STREQUAL "ch32v307")
        target_include_directories(xusbd PRIVATE
            ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32v307/EVT/EXAM/SRC/Peripheral/inc
            ${PROJECT_SOURCE_DIR}/src/third_party/wch/ch32v307/EVT/EXAM/SRC/Core
        )
    endif()
else()
    message(STATUS "xusbd: no port selected - set XUSBD_PORT (e.g. am243x or fake) to include a driver")
endif()

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()

if(xSDK_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()
