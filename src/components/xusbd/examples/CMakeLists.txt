add_library(xusbd_examples STATIC
    cdc_acm/xusbd_cdc_acm_example.c
    cdc_ecm/xusbd_cdc_ecm_example.c
    cdc_ncm/xusbd_cdc_ncm_example.c
    dfu_mode/xusbd_dfu_mode_example.c
    dfu_runtime/xusbd_dfu_runtime_example.c
    hid_custom/xusbd_hid_custom_example.c
    hid_keyboard/xusbd_hid_keyboard_example.c
    hid_mouse/xusbd_hid_mouse_example.c
    msc_ramdisk/xusbd_msc_ramdisk_example.c
    winusb/xusbd_winusb_example.c
)

target_include_directories(xusbd_examples PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cdc_acm
    ${CMAKE_CURRENT_SOURCE_DIR}/cdc_ecm
    ${CMAKE_CURRENT_SOURCE_DIR}/cdc_ncm
    ${CMAKE_CURRENT_SOURCE_DIR}/dfu_mode
    ${CMAKE_CURRENT_SOURCE_DIR}/dfu_runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/hid_custom
    ${CMAKE_CURRENT_SOURCE_DIR}/hid_keyboard
    ${CMAKE_CURRENT_SOURCE_DIR}/hid_mouse
    ${CMAKE_CURRENT_SOURCE_DIR}/msc_ramdisk
    ${CMAKE_CURRENT_SOURCE_DIR}/winusb
)

target_link_libraries(xusbd_examples PUBLIC xsdk::xusbd)

xsdk_apply_target_warnings(xusbd_examples)
