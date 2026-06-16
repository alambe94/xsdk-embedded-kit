add_library(xshell STATIC
    src/xcmd.c
    src/xcli.c
    src/xshell.c
)

add_library(xsdk::xshell ALIAS xshell)

target_include_directories(xshell PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(xshell PUBLIC xsdk::xutil)

xsdk_apply_target_warnings(xshell)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
