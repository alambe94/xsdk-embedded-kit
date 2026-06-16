add_library(xusbh STATIC
    src/class/xusbh_hid.c
    src/class/xusbh_msc.c
    src/core/xusbh_class.c
    src/core/xusbh_core.c
    src/core/xusbh_descriptor.c
    src/core/xusbh_enum.c
)

add_library(xsdk::xusbh ALIAS xusbh)

target_include_directories(xusbh
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xusbh PUBLIC xsdk::xutil xsdk::xtrace xsdk::xusb_common)

xsdk_apply_target_warnings(xusbh)

# Port selection - set XUSBH_PORT to the MCU family or fake to include its HCD.
# Example: cmake -DXUSBH_PORT=am64x
# Example: cmake -DXUSBH_PORT=fake
if(DEFINED XUSBH_PORT)
    if(XUSBH_PORT STREQUAL "fake")
        set(XUSBH_PORT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/port/fake/xusbh_drv.c")
        set(XUSBH_PORT_INC "${CMAKE_CURRENT_SOURCE_DIR}/port/fake")
    else()
        set(XUSBH_PORT_SRC "${CMAKE_CURRENT_SOURCE_DIR}/port/hil/${XUSBH_PORT}/xusbh_drv.c")
        set(XUSBH_PORT_INC "${CMAKE_CURRENT_SOURCE_DIR}/port/hil/${XUSBH_PORT}")
    endif()

    target_sources(xusbh PRIVATE ${XUSBH_PORT_SRC})
    target_include_directories(xusbh PUBLIC ${XUSBH_PORT_INC})

    if(XUSBH_PORT STREQUAL "am64x")
        option(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK "Enable AM64x MCU+ SDK Cadence USBSSP/xHCI binding for xUSBH" OFF)
        option(xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED "Enable AM64x USB3 SERDES/WIZ/Torrent bring-up for xUSBH" OFF)
        set(xUSBH_AM64X_MCU_PLUS_SDK_PATH "${PROJECT_SOURCE_DIR}/src/third_party/ti/mcupsdk_core" CACHE PATH "Path to MCU+ SDK root for AM64x xUSBH port includes")

        if(xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK)
            target_compile_definitions(xusbh PRIVATE
                SOC_AM243X=1
                xUSBH_AM64X_HCD_ENABLE_MCU_PLUS_SDK=1
            )
            if(xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED)
                target_compile_definitions(xusbh PRIVATE
                    xUSBH_AM64X_HCD_ENABLE_SUPER_SPEED=1
                )
                target_sources(xusbh PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xusbd/port/hil/am64x/am64x_wiz.c
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xusbd/port/hil/am64x/am64x_torrent.c
                )
                target_include_directories(xusbh PRIVATE
                    ${CMAKE_CURRENT_SOURCE_DIR}/../xusbd/port/hil/am64x
                )
            endif()
            if(xUSBH_AM64X_MCU_PLUS_SDK_PATH)
                target_include_directories(xusbh PRIVATE
                    ${xUSBH_AM64X_MCU_PLUS_SDK_PATH}/source
                    ${xUSBH_AM64X_MCU_PLUS_SDK_PATH}/source/usb/cdn/core_driver/common/include
                    ${xUSBH_AM64X_MCU_PLUS_SDK_PATH}/source/usb/cdn/core_driver/common/src
                    ${xUSBH_AM64X_MCU_PLUS_SDK_PATH}/source/usb/cdn/core_driver/host/include
                    ${xUSBH_AM64X_MCU_PLUS_SDK_PATH}/source/usb/cdn/core_driver/host/src
                    ${xUSBH_AM64X_MCU_PLUS_SDK_PATH}/source/usb/cdn/include
                )
            endif()
        endif()
    endif()
else()
    message(STATUS "xusbh: no port selected - set XUSBH_PORT (e.g. am64x or fake) to include a driver")
endif()

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
