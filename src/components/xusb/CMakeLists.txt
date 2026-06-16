add_library(xusb_common INTERFACE)
add_library(xsdk::xusb_common ALIAS xusb_common)

target_include_directories(xusb_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xusb_common INTERFACE xsdk::xutil)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
