add_library(xlogic STATIC
    src/xlogic_pru.c
    src/xlogic_sump.c
    src/xlogic_transport.c
    src/xlogic_core.c
)

add_library(xlogic::xlogic ALIAS xlogic)

target_include_directories(xlogic PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# xlogic depends on xutil for xassert.h / xlog.h / xreturn.h
target_link_libraries(xlogic PUBLIC xsdk::xutil)

xsdk_apply_target_warnings(xlogic)

if(xSDK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
